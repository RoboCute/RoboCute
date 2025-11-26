#include "http_client.h"
#include <luisa/core/logging.h>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace rbc {

HttpClient::HttpClient()
    : server_url_("http://127.0.0.1:5555"),
      host_("127.0.0.1"),
      port_(5555) {
}

HttpClient::~HttpClient() = default;

void HttpClient::set_server_url(const luisa::string &url) {
    server_url_ = url;
    parse_url(url);
}

void HttpClient::parse_url(const luisa::string &url) {
    // Simple URL parser for http://host:port format
    size_t protocol_end = url.find("://");
    if (protocol_end == luisa::string::npos) {
        LUISA_ERROR("Invalid URL format: {}", url);
        return;
    }

    size_t host_start = protocol_end + 3;
    size_t port_start = url.find(':', host_start);
    size_t path_start = url.find('/', host_start);

    if (port_start != luisa::string::npos && (path_start == luisa::string::npos || port_start < path_start)) {
        host_ = url.substr(host_start, port_start - host_start);
        size_t port_end = (path_start != luisa::string::npos) ? path_start : url.length();
        luisa::string port_str = url.substr(port_start + 1, port_end - port_start - 1);
        port_ = std::stoi(port_str.c_str());
    } else if (path_start != luisa::string::npos) {
        host_ = url.substr(host_start, path_start - host_start);
        port_ = 80;
    } else {
        host_ = url.substr(host_start);
        port_ = 80;
    }
}

luisa::string HttpClient::http_request(const luisa::string &method,
                                      const luisa::string &endpoint,
                                      const luisa::string &body) {
#ifdef _WIN32
    // Convert strings to wide strings for WinHTTP
    int host_size = MultiByteToWideChar(CP_UTF8, 0, host_.c_str(), -1, NULL, 0);
    luisa::vector<wchar_t> w_host(host_size);
    MultiByteToWideChar(CP_UTF8, 0, host_.c_str(), -1, w_host.data(), host_size);

    int endpoint_size = MultiByteToWideChar(CP_UTF8, 0, endpoint.c_str(), -1, NULL, 0);
    luisa::vector<wchar_t> w_endpoint(endpoint_size);
    MultiByteToWideChar(CP_UTF8, 0, endpoint.c_str(), -1, w_endpoint.data(), endpoint_size);

    int method_size = MultiByteToWideChar(CP_UTF8, 0, method.c_str(), -1, NULL, 0);
    luisa::vector<wchar_t> w_method(method_size);
    MultiByteToWideChar(CP_UTF8, 0, method.c_str(), -1, w_method.data(), method_size);

    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(
        L"RoboCute Editor/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!hSession) {
        LUISA_WARNING("WinHttpOpen failed: {}", GetLastError());
        return "";
    }

    // Connect to server
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        w_host.data(),
        port_,
        0);

    if (!hConnect) {
        LUISA_WARNING("WinHttpConnect failed: {}", GetLastError());
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Create request
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        w_method.data(),
        w_endpoint.data(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);

    if (!hRequest) {
        LUISA_WARNING("WinHttpOpenRequest failed: {}", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Set headers
    luisa::string headers = "Content-Type: application/json\r\n";
    int headers_size = MultiByteToWideChar(CP_UTF8, 0, headers.c_str(), -1, NULL, 0);
    luisa::vector<wchar_t> w_headers(headers_size);
    MultiByteToWideChar(CP_UTF8, 0, headers.c_str(), -1, w_headers.data(), headers_size);

    // Send request
    BOOL result = WinHttpSendRequest(
        hRequest,
        w_headers.data(),
        -1,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
        body.length(),
        body.length(),
        0);

    if (!result) {
        LUISA_WARNING("WinHttpSendRequest failed: {}", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Receive response
    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
        LUISA_WARNING("WinHttpReceiveResponse failed: {}", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Read response data
    luisa::string response;
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;
    luisa::vector<char> buffer;

    do {
        bytes_available = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytes_available)) {
            LUISA_WARNING("WinHttpQueryDataAvailable failed: {}", GetLastError());
            break;
        }

        if (bytes_available > 0) {
            buffer.resize(bytes_available + 1);
            if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read)) {
                buffer[bytes_read] = '\0';
                response += buffer.data();
            }
        }
    } while (bytes_available > 0);

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
#else
    LUISA_ERROR("HTTP client only supports Windows platform");
    return "";
#endif
}

luisa::string HttpClient::get(const luisa::string &endpoint) {
    return http_request("GET", endpoint, "");
}

luisa::string HttpClient::post(const luisa::string &endpoint, const luisa::string &json_body) {
    return http_request("POST", endpoint, json_body);
}

luisa::string HttpClient::get_scene_state() {
    return get("/scene/state");
}

luisa::string HttpClient::get_all_resources() {
    return get("/resources/all");
}

luisa::string HttpClient::get_resource(int resource_id) {
    std::ostringstream oss;
    oss << "/resources/" << resource_id;
    return get(oss.str().c_str());
}

bool HttpClient::send_heartbeat(const luisa::string &editor_id) {
    std::ostringstream oss;
    oss << "{\"editor_id\":\"" << editor_id.c_str() << "\"}";
    luisa::string response = post("/editor/heartbeat", oss.str().c_str());
    return !response.empty() && response.find("\"success\":true") != luisa::string::npos;
}

bool HttpClient::register_editor(const luisa::string &editor_id) {
    std::ostringstream oss;
    oss << "{\"editor_id\":\"" << editor_id.c_str() << "\"}";
    luisa::string response = post("/editor/register", oss.str().c_str());
    return !response.empty() && response.find("\"success\":true") != luisa::string::npos;
}

bool HttpClient::is_connected() {
    luisa::string response = get("/scene/state");
    return !response.empty();
}

}// namespace rbc

