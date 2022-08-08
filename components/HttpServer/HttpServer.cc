#include "HttpServer.h"
#include "WebSockets.h"
#include <esp_log.h>
#include <functional>

static const char* TAG = "httpd";

namespace {

int ctoh(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return (c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return (c - 'A') + 10;
    }
    return -1;
}

} // namespace
namespace httpd {

std::string urldecode(std::string_view in)
{
    std::string out;
    out.reserve(in.size());
    auto len = in.size();
    auto i = 0;
    while (i < len) {
        if (in[i] == '+') {
            out.push_back(' ');
            ++i;
            continue;
        }
        if (in[i] == '%' && (i + 3) < len) {
            int h = ctoh(in[i+1]);
            int l = ctoh(in[i+2]);
            if (h >= 0 && l >= 0) {
                out.push_back((char)((h << 4) | l));
                i += 3;
                continue;
            }
        }
        out.push_back(in[i]);
        ++i;
    }
    return out;
}

std::string Request::requestHeader(const std::string& name) const
{
    std::string out;

    size_t len = httpd_req_get_hdr_value_len(request_, name.c_str());
    if (len) {
        out.resize(len);
        httpd_req_get_hdr_value_str(request_, name.c_str(), out.data(), out.size() + 1);
    }
    return out;
}

void
Request::setContentType(const std::string& type)
{
    httpd_resp_set_type(request_, type.c_str());
}
void
Request::sendResponse(std::string_view data)
{
    httpd_resp_send(request_, data.data(), data.length());
}

void
Request::sendResponse(const char* data, ssize_t len)
{
    httpd_resp_send(request_, data, len);
}

void
Request::sendError(httpd_err_code_t err)
{
    httpd_resp_send_err(request_, err, nullptr);
}

void
Request::sendError(httpd_err_code_t err, const std::string& msg)
{
    httpd_resp_send_err(request_, err, msg.c_str());
}

Server::Server()
    : handle_(nullptr)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);

    if (httpd_start(&handle_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server!");
    }

}

Server::~Server() {
    if (handle_) {
        httpd_stop(handle_);
    }
}

void
Server::registerHandler(const std::string& uri, RequestHandler& handler)
{
    // There doesn't seem to be a generated end to the http_method enum, and
    // practically for now we only care about a few, so just look for them.
    static const httpd_method_t SUPPORTED_METHODS[] = {
        HTTP_DELETE, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_PATCH
    };

    httpd_uri_t reg = {};
    reg.uri = uri.c_str();
    reg.handler = Server::dispatch;
    reg.user_ctx = &handler;

    WSHandler* wsh = dynamic_cast<WSHandler*>(&handler);
    if (wsh) {
        ESP_LOGI(TAG, "WebSocket URI %s", uri.c_str());
        reg.method = HTTP_GET;
        reg.is_websocket = true;
        if (!wsh->subprotocols().empty()) {
            reg.supported_subprotocol = wsh->subprotocols().c_str();
        }
        reg.handle_ws_control_frames = wsh->controlFrames();
        esp_err_t rv = httpd_register_uri_handler(handle_, &reg);
        if (ESP_OK != rv) {
            ESP_LOGI(TAG, "Failed registering WebSocket handler for %s: %d", uri.c_str(), rv);
        }
        return;
    }

    for (httpd_method_t m : SUPPORTED_METHODS) {
        if (handler.canHandle(m)) {
            reg.method = m;
            esp_err_t rv = httpd_register_uri_handler(handle_, &reg);
            if (ESP_OK != rv) {
                ESP_LOGI(TAG,
                         "Failed registering handler for %s: %d",
                         uri.c_str(),
                         rv);
            }
        }
    };
}

esp_err_t
Server::dispatch(httpd_req_t* req)
{
    ESP_LOGI(TAG, "Dispatch method %d handler %p", req->method, req->user_ctx);
    RequestHandler* h = static_cast<RequestHandler*>(req->user_ctx);
    return h->handle(req);
}

} // namespace httpd
