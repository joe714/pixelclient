#include "WebSockets.h"
#include <esp_log.h>
#include <esp_http_server.h>

static const char* TAG = "WebSockets";

namespace httpd {

esp_err_t WSSocket::send(std::string_view str, httpd_ws_type_t type)
{
    return send((uint8_t*)str.data(), str.size(), type);
}

esp_err_t WSSocket::send(const uint8_t* payload, size_t len, httpd_ws_type_t type)
{
    WSFrame frame = { .final = true,
                   .fragmented = false,
                   .type = type,
                   .payload = const_cast<uint8_t*>(payload),
                   .len = len };
    return send(frame);
}

esp_err_t WSSocket::send(const WSFrame& frame)
{
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(server_, fd_);
    if (info != HTTPD_WS_CLIENT_WEBSOCKET) {
        ESP_LOGI(TAG, "Send on closed socket! %d", info);
        return ESP_FAIL;
    }

    esp_err_t rv = httpd_ws_send_frame_async(server_, fd_, const_cast<WSFrame*>(&frame));
    return rv;
}

WSHandler::WSHandler(const std::string& subprotocols, bool controlFrames)
  : subprotocols_(subprotocols), controlFrames_(controlFrames)
{
}

bool WSHandler::canHandle(httpd_method_t m) const
{
    return m == HTTP_GET;
}

esp_err_t
WSHandler::handle(httpd_req_t* req)
{
    WSSocket sock(*this, req->handle, httpd_req_to_sockfd(req));
    ESP_LOGI(TAG, "WSHandler::handle fd: %p %p %d", this, sock.server(), sock.fd());

    esp_err_t rv = ESP_FAIL;
    if (HTTP_GET == req->method) {
        ESP_LOGI(TAG, "onConnect");
        rv = onConnect(sock);
        ESP_LOGI(TAG, "onConnect result: %d", rv);
        return rv;
    }

    WSFrame frame = {};
    rv = httpd_ws_recv_frame(req, &frame, 0);
    if (ESP_OK != rv) {
        ESP_LOGE(TAG, "Failed to read frame length from fd %d: %d",
                sock.fd(), rv);
        onClose(sock);
        return rv;
    }

    if (frame.len > 2048) {
        ESP_LOGI(TAG, "Frame too long: %d", frame.len);
        onClose(sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Calling httpd_ws_recv_frame for len %d", frame.len);
    std::vector<uint8_t> payload(frame.len);
    frame.payload = payload.data();
    rv = httpd_ws_recv_frame(req, &frame, payload.size());
    if (ESP_OK != rv) {
        ESP_LOGI(TAG, "httpd_ws_recv_frame error: %d", rv);
    } else {
        ESP_LOGI(TAG, "Handle onReceive");
        rv = onReceive(sock, frame);
        ESP_LOGI(TAG, "onReceive result: %d", rv);
    }

    if (ESP_OK != rv) {
        onClose(sock);
    }

    return rv;
}

} // namespace httpd

