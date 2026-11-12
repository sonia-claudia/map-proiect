// Contact book service: pure logic lives here so it can be unit tested without starting a server.
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace map_project {

inline constexpr const char* kAppName = "map-project";
inline constexpr const char* kAppVersion = "0.1.0";

/// Maximum number of characters allowed in a contact name.
inline constexpr std::size_t kMaxNameLength = 50;

inline const std::array<std::string, 4> kCategories = {"family", "friends", "work", "other"};

/// Reads an environment variable, falling back when it is missing or empty.
inline std::string Env(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return (value == nullptr || *value == '\0') ? fallback : std::string(value);
}

struct Contact {
    int id = 0;
    std::string name;
    std::string email;
    std::string phone;
    std::string category;
};

/// An HTTP-like outcome produced by the pure logic, so it can be tested without a server.
struct Result {
    int status = 200;
    nlohmann::json body;
};

inline nlohmann::json ErrorBody(const std::string& code, const std::string& message) {
    return {{"error", code}, {"message", message}};
}

inline Result Failure(int status, const std::string& code, const std::string& message) {
    return {status, ErrorBody(code, message)};
}

/// Counts Unicode characters in a UTF-8 string (continuation bytes are skipped).
inline std::size_t CharCount(const std::string& text) {
    std::size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

/// ASCII lower-casing, used only to compare values, never to store them.
inline std::string Lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

inline bool IsValidName(const std::string& name) {
    const std::size_t length = CharCount(name);
    return length >= 1 && length <= kMaxNameLength;
}

/// Exactly one '@', some text before it and at least one '.' after it.
inline bool IsValidEmail(const std::string& email) {
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0) return false;
    if (email.find('@', at + 1) != std::string::npos) return false;
    return email.find('.', at + 1) != std::string::npos;
}

/// Exactly 10 digits, optionally preceded by a single '+'.
inline bool IsValidPhone(const std::string& phone) {
    const std::size_t start = (!phone.empty() && phone[0] == '+') ? 1 : 0;
    if (phone.size() - start != 10) return false;
    for (std::size_t i = start; i < phone.size(); ++i) {
        if (phone[i] < '0' || phone[i] > '9') return false;
    }
    return true;
}

inline bool IsValidCategory(const std::string& category) {
    return std::find(kCategories.begin(), kCategories.end(), category) != kCategories.end();
}

inline nlohmann::json ContactToJson(const Contact& contact) {
    return {{"id", contact.id},
            {"name", contact.name},
            {"email", contact.email},
            {"phone", contact.phone},
            {"category", contact.category}};
}

/// Holds the application data in memory.
/// Everything is lost when the container restarts.
class Store {
public:
    void Reset() {
        std::lock_guard<std::mutex> guard(mutex_);
        next_id_ = 1;
        contacts_.clear();
    }

    int NextId() {
        std::lock_guard<std::mutex> guard(mutex_);
        return next_id_;
    }

    int TakeNextId() {
        std::lock_guard<std::mutex> guard(mutex_);
        return next_id_++;
    }

    /// Validates the JSON body and stores a new contact.
    Result AddContact(const std::string& raw_body) {
        const auto body = nlohmann::json::parse(raw_body, nullptr, false);
        if (!body.is_object()) {
            return Failure(400, "validation", "body must be a JSON object");
        }

        for (const char* field : {"name", "email", "phone", "category"}) {
            if (!body.contains(field) || !body[field].is_string()) {
                return Failure(400, "validation",
                               std::string(field) + " is required and must be a string");
            }
        }

        Contact contact;
        contact.name = body["name"].get<std::string>();
        contact.email = body["email"].get<std::string>();
        contact.phone = body["phone"].get<std::string>();
        contact.category = body["category"].get<std::string>();

        if (!IsValidName(contact.name)) {
            return Failure(400, "validation", "name must have between 1 and 50 characters");
        }
        if (!IsValidEmail(contact.email)) {
            return Failure(400, "validation", "email is not valid");
        }
        if (!IsValidPhone(contact.phone)) {
            return Failure(400, "validation",
                           "phone must have exactly 10 digits, optionally preceded by +");
        }
        if (!IsValidCategory(contact.category)) {
            return Failure(400, "validation", "category must be one of family, friends, work, other");
        }

        std::lock_guard<std::mutex> guard(mutex_);
        // Compare the normalized email, but store the one the user typed.
        const std::string normalized = Lower(contact.email);
        for (const auto& entry : contacts_) {
            if (Lower(entry.second.email) == normalized) {
                return Failure(409, "conflict", "a contact with this email already exists");
            }
        }
        contact.id = next_id_++;
        contacts_[contact.id] = contact;
        return {201, ContactToJson(contact)};
    }

    Result GetContact(int id) {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto found = contacts_.find(id);
        if (found == contacts_.end()) return Failure(404, "not_found", "contact does not exist");
        return {200, ContactToJson(found->second)};
    }

    Result DeleteContact(int id) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (contacts_.erase(id) == 0) return Failure(404, "not_found", "contact does not exist");
        return {204, nlohmann::json()};
    }

    /// When both filters are present, both are applied. Sorted by name, then by id.
    Result ListContacts(const std::string& category, const std::string& query) {
        if (!category.empty() && !IsValidCategory(category)) {
            return Failure(400, "validation", "category must be one of family, friends, work, other");
        }
        const std::string needle = Lower(query);

        std::lock_guard<std::mutex> guard(mutex_);
        std::vector<const Contact*> matches;
        for (const auto& entry : contacts_) {
            const Contact& contact = entry.second;
            if (!category.empty() && contact.category != category) continue;
            if (!needle.empty() && Lower(contact.name).find(needle) == std::string::npos) continue;
            matches.push_back(&contact);
        }
        std::sort(matches.begin(), matches.end(), [](const Contact* a, const Contact* b) {
            return a->name != b->name ? a->name < b->name : a->id < b->id;
        });

        nlohmann::json list = nlohmann::json::array();
        for (const Contact* contact : matches) list.push_back(ContactToJson(*contact));
        return {200, list};
    }

    Result Stats() {
        std::lock_guard<std::mutex> guard(mutex_);
        nlohmann::json by_category = nlohmann::json::object();
        for (const auto& category : kCategories) by_category[category] = 0;
        for (const auto& entry : contacts_) {
            auto& counter = by_category[entry.second.category];
            counter = counter.get<int>() + 1;
        }
        return {200, {{"total", contacts_.size()}, {"by_category", by_category}}};
    }

private:
    std::mutex mutex_;
    int next_id_ = 1;
    std::map<int, Contact> contacts_;
};

/// Seconds since the process started.
inline long UptimeSeconds(std::chrono::steady_clock::time_point started_at) {
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
}

inline nlohmann::json HealthBody(std::chrono::steady_clock::time_point started_at) {
    return {{"status", "ok"}, {"uptime_seconds", UptimeSeconds(started_at)}};
}

inline nlohmann::json VersionBody() {
    return {
        {"app", kAppName},
        {"version", kAppVersion},
        {"commit", Env("APP_COMMIT", "dev")},
        {"built_at", Env("APP_BUILT_AT", "unknown")},
    };
}

inline std::string HomePage() {
    return std::string(R"(<!DOCTYPE html>
<html lang="ro"><head><meta charset="utf-8"><title>)") + kAppName + R"(</title></head>
<body>
<h1>)" + kAppName + R"(</h1>
<p>Autor: Suceava Sonia Claudia, grupa 2.2</p>
<p>Tema: 1 </p>
<p>Versiune: )" + kAppVersion + ", commit " + Env("APP_COMMIT", "dev") + R"(</p>
</body></html>)";
}

}  // namespace map_project
