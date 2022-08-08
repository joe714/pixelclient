#pragma once

#include "HttpServer.h"
#include <string_view>

#ifdef CONFIG_HTTPD_WS_SUPPORT

namespace httpd {
class WSHandler;

using WSFrame = httpd_ws_frame_t;

class WSSocket {
public:
    WSHandler& handler() { return handler_; }
    httpd_handle_t server() const { return server_; }
    int fd() const { return fd_; }

    esp_err_t send(std::string_view str, httpd_ws_type_t type = HTTPD_WS_TYPE_TEXT);
    esp_err_t send(const uint8_t* data, size_t len, httpd_ws_type_t type = HTTPD_WS_TYPE_BINARY);
    esp_err_t send(const WSFrame& frame);

private:
    friend class WSHandler;

    WSSocket(WSHandler& handler, httpd_handle_t server, int fd)
      : handler_(handler), server_(server), fd_(fd){};

    WSHandler& handler_;
    httpd_handle_t server_;
    int fd_;
};

class WSHandler : public RequestHandler {
public:
    WSHandler(const std::string& subprotocols = std::string(),
                     bool controlFrames = false);

    const std::string& subprotocols() const { return subprotocols_; }
    bool controlFrames() const { return controlFrames_; }

    virtual bool canHandle(httpd_method_t m) const final;
    virtual esp_err_t handle(httpd_req_t* req) override;

    virtual esp_err_t onConnect(WSSocket&) = 0;
    virtual esp_err_t onReceive(WSSocket&, const WSFrame&) = 0;
    virtual void onClose(WSSocket&) {}

private:
    std::string subprotocols_;
    bool controlFrames_;
};


} // namespace httpd

#endif // CONFIG_HTTPD_WS_SUPPORT
