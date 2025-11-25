#pragma once
#include <string>
#include <functional>
#include <memory>
#include <luisa/vstl/common.h>

namespace rbc {

/**
 * Simple HTTP client for communicating with Python server
 * Uses Windows WinHTTP API
 */
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Set server URL (default: http://127.0.0.1:5555)
    void set_server_url(const luisa::string &url);
    luisa::string server_url() const { return server_url_; }

    // GET request - returns JSON response as string
    // Returns empty string on error
    luisa::string get(const luisa::string &endpoint);

    // POST request with JSON body
    // Returns JSON response as string
    luisa::string post(const luisa::string &endpoint, const luisa::string &json_body);

    // Convenience methods for common endpoints
    luisa::string get_scene_state();
    luisa::string get_all_resources();
    luisa::string get_resource(int resource_id);
    bool send_heartbeat(const luisa::string &editor_id);
    bool register_editor(const luisa::string &editor_id);

    // Check if server is reachable
    bool is_connected();

private:
    luisa::string server_url_;
    luisa::string host_;
    int port_;

    void parse_url(const luisa::string &url);
    luisa::string http_request(const luisa::string &method,
                               const luisa::string &endpoint,
                               const luisa::string &body = "");
};

}// namespace rbc

