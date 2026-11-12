// Wires the HTTP routes of the common contract.
// Add the routes required by your assigned theme below.

#include <chrono>
#include <iostream>

#include <httplib.h>

#include "app.hpp"

int main() {
    using namespace map_project;

    const auto started_at = std::chrono::steady_clock::now();
    Store store;
    httplib::Server server;

    server.Get("/health", [&](const httplib::Request&, httplib::Response& response) {
        response.set_content(HealthBody(started_at).dump(), "application/json; charset=utf-8");
    });

    server.Get("/version", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(VersionBody().dump(), "application/json; charset=utf-8");
    });

    server.Post("/reset", [&](const httplib::Request&, httplib::Response& response) {
        store.Reset();
        response.status = 204;
    });

    const auto send = [](httplib::Response& response, const Result& result) {
        response.status = result.status;
        if (result.status != 204) {
            response.set_content(result.body.dump(), "application/json; charset=utf-8");
        }
    };

    // The route regex only matches digits; a value too large for int is treated as missing.
    const auto parse_id = [](const httplib::Request& request) {
        try {
            return std::stoi(request.matches[1].str());
        } catch (...) {
            return -1;
        }
    };

    server.Post("/contacts", [&](const httplib::Request& request, httplib::Response& response) {
        send(response, store.AddContact(request.body));
    });

    server.Get("/contacts", [&](const httplib::Request& request, httplib::Response& response) {
        const std::string category =
            request.has_param("category") ? request.get_param_value("category") : "";
        const std::string query = request.has_param("q") ? request.get_param_value("q") : "";
        send(response, store.ListContacts(category, query));
    });

    server.Get(R"(/contacts/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
        send(response, store.GetContact(parse_id(request)));
    });

    server.Delete(R"(/contacts/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
        send(response, store.DeleteContact(parse_id(request)));
    });

    server.Get("/stats", [&](const httplib::Request&, httplib::Response& response) {
        send(response, store.Stats());
    });

    server.Get("/", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(HomePage(), "text/html; charset=utf-8");
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        // Routes that already produced their own error body keep it.
        if (response.status == 404 && response.body.empty()) {
            response.set_content(ErrorBody("not_found", "route does not exist").dump(),
                                 "application/json; charset=utf-8");
        }
    });

    std::cout << kAppName << " " << kAppVersion << " starting on port 8080" << std::endl;
    if (!server.listen("0.0.0.0", 8080)) {
        std::cerr << "failed to bind port 8080" << std::endl;
        return 1;
    }
    return 0;
}
