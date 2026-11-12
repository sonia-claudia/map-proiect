#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>

#include "app.hpp"

using namespace map_project;

TEST_CASE("health body reports ok and uptime") {
    const auto started_at = std::chrono::steady_clock::now();
    const auto body = HealthBody(started_at);

    CHECK(body["status"] == "ok");
    CHECK(body.contains("uptime_seconds"));
    CHECK(body["uptime_seconds"].get<long>() >= 0);
}

TEST_CASE("version body contains all required fields") {
    const auto body = VersionBody();

    CHECK(body["app"] == kAppName);
    CHECK(body["version"] == kAppVersion);
    CHECK(body.contains("commit"));
    CHECK(body.contains("built_at"));
}

TEST_CASE("env falls back when the variable is missing") {
    CHECK(Env("MAP_VARIABLE_THAT_DOES_NOT_EXIST", "fallback") == "fallback");
}

TEST_CASE("error body has the standard shape") {
    const auto body = ErrorBody("not_found", "route does not exist");

    CHECK(body["error"] == "not_found");
    CHECK(body["message"] == "route does not exist");
}

TEST_CASE("new store starts at one") {
    Store store;
    CHECK(store.NextId() == 1);
}

TEST_CASE("take next id increments") {
    Store store;

    CHECK(store.TakeNextId() == 1);
    CHECK(store.TakeNextId() == 2);
    CHECK(store.NextId() == 3);
}

TEST_CASE("reset returns to one") {
    Store store;
    store.TakeNextId();
    store.TakeNextId();

    store.Reset();

    CHECK(store.NextId() == 1);
}

TEST_CASE("home page mentions the application name") {
    CHECK(HomePage().find(kAppName) != std::string::npos);
}

namespace {
std::string ContactJson(const std::string& name, const std::string& email,
                        const std::string& phone = "0722334455",
                        const std::string& category = "work") {
    return nlohmann::json{{"name", name}, {"email", email}, {"phone", phone}, {"category", category}}
        .dump();
}
}  // namespace

TEST_CASE("name accepts exactly 50 characters and rejects 51") {
    CHECK(IsValidName(std::string(50, 'a')));
    CHECK_FALSE(IsValidName(std::string(51, 'a')));
    CHECK_FALSE(IsValidName(""));
}

TEST_CASE("name length counts characters, not bytes") {
    std::string accented;
    for (int i = 0; i < 50; ++i) accented += "\xC4\x83";  // 50 two-byte characters
    CHECK(IsValidName(accented));
    CHECK_FALSE(IsValidName(accented + "\xC4\x83"));
}

TEST_CASE("email needs exactly one at sign, text before it and a dot after it") {
    CHECK(IsValidEmail("ana@example.com"));
    CHECK_FALSE(IsValidEmail("ana.example.com"));
    CHECK_FALSE(IsValidEmail("@example.com"));
    CHECK_FALSE(IsValidEmail("ana@examplecom"));
    CHECK_FALSE(IsValidEmail("a@b@example.com"));
}

TEST_CASE("phone needs exactly ten digits with an optional plus") {
    CHECK(IsValidPhone("0722334455"));
    CHECK(IsValidPhone("+0722334455"));
    CHECK_FALSE(IsValidPhone("072233445"));
    CHECK_FALSE(IsValidPhone("07223344556"));
    CHECK_FALSE(IsValidPhone("07223344a5"));
    CHECK_FALSE(IsValidPhone(""));
}

TEST_CASE("category must be one of the four allowed values") {
    for (const auto& category : kCategories) CHECK(IsValidCategory(category));
    CHECK_FALSE(IsValidCategory("Work"));
    CHECK_FALSE(IsValidCategory("boss"));
}

TEST_CASE("adding a contact returns 201 with an id starting at one") {
    Store store;
    const auto result = store.AddContact(ContactJson("Ana Pop", "ana@example.com"));

    CHECK(result.status == 201);
    CHECK(result.body["id"] == 1);
    CHECK(result.body["name"] == "Ana Pop");
}

TEST_CASE("invalid input is rejected with 400 validation") {
    Store store;

    CHECK(store.AddContact("not json").status == 400);
    CHECK(store.AddContact("[]").status == 400);
    CHECK(store.AddContact(R"({"name":"Ana"})").status == 400);
    CHECK(store.AddContact(ContactJson("", "ana@example.com")).status == 400);
    CHECK(store.AddContact(ContactJson("Ana", "ana@example.com", "123")).status == 400);
    CHECK(store.AddContact(ContactJson("Ana", "ana@example.com", "0722334455", "boss")).status == 400);
    const auto wrong_type = store.AddContact(R"({"name":1,"email":"a@b.c","phone":"0722334455","category":"work"})");
    CHECK(wrong_type.status == 400);
    CHECK(wrong_type.body["error"] == "validation");
}

TEST_CASE("a failed validation does not consume an id") {
    Store store;
    store.AddContact(ContactJson("", "ana@example.com"));

    CHECK(store.AddContact(ContactJson("Ana", "ana@example.com")).body["id"] == 1);
}

TEST_CASE("duplicate email is a conflict regardless of case") {
    Store store;
    store.AddContact(ContactJson("Ana Pop", "ana@example.com"));

    const auto duplicate = store.AddContact(ContactJson("Ana P.", "Ana@Example.COM", "0733445566", "friends"));

    CHECK(duplicate.status == 409);
    CHECK(duplicate.body["error"] == "conflict");
}

TEST_CASE("the email is returned exactly as the user wrote it") {
    Store store;
    const auto created = store.AddContact(ContactJson("Ana", "Ana@Example.com"));

    CHECK(created.body["email"] == "Ana@Example.com");
    CHECK(store.GetContact(1).body["email"] == "Ana@Example.com");
}

TEST_CASE("unknown contact gives 404 on get and delete") {
    Store store;

    CHECK(store.GetContact(999).status == 404);
    CHECK(store.GetContact(999).body["error"] == "not_found");
    CHECK(store.DeleteContact(999).status == 404);
}

TEST_CASE("delete removes the contact and returns 204") {
    Store store;
    store.AddContact(ContactJson("Ana", "ana@example.com"));

    CHECK(store.DeleteContact(1).status == 204);
    CHECK(store.GetContact(1).status == 404);
}

TEST_CASE("list is sorted by name, ties by id") {
    Store store;
    store.AddContact(ContactJson("Zed", "z@example.com"));
    store.AddContact(ContactJson("Ana", "a1@example.com"));
    store.AddContact(ContactJson("Ana", "a2@example.com"));

    const auto list = store.ListContacts("", "").body;

    CHECK(list.size() == 3);
    CHECK(list[0]["id"] == 2);
    CHECK(list[1]["id"] == 3);
    CHECK(list[2]["id"] == 1);
}

TEST_CASE("category filter and name search are combined") {
    Store store;
    store.AddContact(ContactJson("Ana Pop", "a@example.com", "0722334455", "work"));
    store.AddContact(ContactJson("Ana Ion", "b@example.com", "0722334456", "friends"));
    store.AddContact(ContactJson("Dan", "c@example.com", "0722334457", "work"));

    const auto both = store.ListContacts("work", "ANA").body;

    CHECK(both.size() == 1);
    CHECK(both[0]["name"] == "Ana Pop");
    CHECK(store.ListContacts("work", "").body.size() == 2);
    CHECK(store.ListContacts("", "ana").body.size() == 2);
    CHECK(store.ListContacts("boss", "").status == 400);
}

TEST_CASE("stats count contacts per category") {
    Store store;
    store.AddContact(ContactJson("Ana", "a@example.com", "0722334455", "work"));
    store.AddContact(ContactJson("Dan", "b@example.com", "0722334456", "work"));
    store.AddContact(ContactJson("Eva", "c@example.com", "0722334457", "family"));

    const auto stats = store.Stats().body;

    CHECK(stats["total"] == 3);
    CHECK(stats["by_category"]["work"] == 2);
    CHECK(stats["by_category"]["family"] == 1);
    CHECK(stats["by_category"]["other"] == 0);
}

TEST_CASE("reset clears contacts and restarts ids") {
    Store store;
    store.AddContact(ContactJson("Ana", "ana@example.com"));

    store.Reset();

    CHECK(store.ListContacts("", "").body.empty());
    CHECK(store.AddContact(ContactJson("Ana", "ana@example.com")).body["id"] == 1);
}
