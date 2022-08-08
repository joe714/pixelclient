#include "RootPage.h"
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <chrono>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "Config.h"

using namespace httpd;

static const char* TAG = "RootPage";

using namespace std::chrono_literals;

namespace {

TickType_t ticks(std::chrono::milliseconds ms)
{
    return ms.count() / portTICK_PERIOD_MS;
}

TimerHandle_t restart;

void delayedRestart(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Restart");
    esp_restart();
}

} // namespace

esp_err_t
RootPage::onGet(Request& req)
{
    ESP_LOGI(TAG, "get");
    char buf[66] = {};
    std::string resp = R"(
<html>
  <head>
    <meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate" />
    <meta http-equiv="Pragma" content="no-cache" />
    <meta http-equiv="Expires" content="0" />
    <title>OTA Update</title>
    <style>
        table, th, td { border: 1px solid black; }
    </style>
  </head>
  <body>
    <table>
      <thead>
        <tr>
          <th colspan="2">Device Information</th>
        </tr>
      </thead>
      <tbody>
      )";

    resp.append("<tr><th>Device UUID</th><td>")
        .append(Config::instance().deviceUUID())
        .append("</td></tr>\n")
        .append("<tr><th>Endpoint</th><td>")
        .append(Config::instance().endpoint())
        .append("</td></tr>\n");

    uint8_t mac[6] ={};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, sizeof buf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    resp.append("<tr><th>MAC Address</th><td>")
        .append(buf)
        .append("</td></tr>\n");

    snprintf(buf,
             sizeof buf,
             "%lu KB",
             esp_get_free_heap_size() / 1024);
    resp.append("<tr><th>Free Heap</th><td>")
        .append(buf)
        .append("</td></tr>\n");

    snprintf(buf,
             sizeof buf,
             "%lu KB</td></tr>",
             esp_get_minimum_free_heap_size() / 1024);
    resp.append("<tr><th>Min Free Heap</th><td>")
        .append(buf)
        .append("</td></tr>\n");

    resp.append(R"(
      </tbody>
    </table>
    <br/>
    <table>
      <thead>
        <tr>
          <th colspan="2">App Detail</th>
        </tr>
      </thead>
      <tbody>
)");

    const esp_app_desc_t* app = esp_app_get_description();
    const esp_partition_t* part = esp_ota_get_running_partition();

    resp.append("<tr><th>App Version</th><td>")
        .append(app->version)
        .append("</td></tr>\n");

    resp.append("<tr><th>IDF Version</th><td>")
        .append(app->idf_ver)
        .append("</td></tr>\n");

    resp.append("<tr><th>Compiled</th><td>")
        .append(app->date)
        .append(" ")
        .append(app->time)
        .append("</td></tr>\n");

    for (int i = 0; i < 32; ++i) {
        snprintf(&buf[i*2], 3, "%.2x", app->app_elf_sha256[i]);
    }
    resp.append("<tr><th>App SHA256</th><td>")
        .append(buf)
        .append("</td></tr>\n");

    snprintf(buf, sizeof buf, "%s (0x%" PRIX32 ")", part->label, part->address);
    resp.append("<tr><th>Running Partition</th><td>")
        .append(buf)
        .append("</td></tr>\n");


    resp.append(R"(
      </tbody>
    </table>
    <br/>
    <form method="post">
      <label>Firmare URL:<br />
    )");

    resp.append(R"(<input name="firmware" type="url" size="64" value="http://)")
        .append(Config::instance().endpoint())
        .append(R"(:8000/PixelClient.bin" required />)");

    resp.append(R"(
      </label><br />
      <button>Update Firmware</button>
    </form>
  </body>
</html>
)");
    req.sendResponse(resp);
    return ESP_OK;
};

esp_err_t
RootPage::onPost(Request& req)
{
    ESP_LOGI(TAG, "OTA POST");
    using namespace std::string_literals;
    const std::string ctype = req.requestHeader("Content-Type");
            
    if (ctype != "application/x-www-form-urlencoded"s) {
        ESP_LOGI(TAG, "Unsupported Content-Type: '%s'", ctype.c_str());
        httpd_resp_set_status(req.request(), "406 Unsupported Content-Type");
        httpd_resp_set_type(req.request(), "text/plain");
        req.sendResponse("Unsupported Content Type");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "POST %d bytes", req.contentLength());
    char buf[128] = {};
    if (req.contentLength() > sizeof buf) {
        httpd_resp_set_status(req.request(), "406 Invalid Form Data");
        httpd_resp_set_type(req.request(), "text/plain");
        req.sendResponse("Invalid Form Data");
        return ESP_OK;
    }

    int l = httpd_req_recv(req.request(), buf, sizeof buf);
    if (l <= 0) {
        req.sendResponse("Invalid Form Data");
        return ESP_OK;
    }

    std::string_view formData(buf);
    size_t pos = formData.find("firmware=");
    if (std::string_view::npos == pos) {
        ESP_LOGI(TAG, "No firmware value found");
        req.sendResponse("Invalid form data");
        return ESP_OK;
    }
    formData.remove_prefix(9);
    pos = formData.find("&");
    if (pos != std::string_view::npos) {
        formData.remove_suffix(formData.size() - pos);
    }
    std::string uri = urldecode(formData);
    ESP_LOGI(TAG, "Firmware URL: %s", uri.c_str());
    esp_http_client_config_t httpcnf = { };
    httpcnf.url = uri.c_str();
    esp_https_ota_config_t config = {};
    config.http_config = &httpcnf;
    esp_err_t rv = esp_https_ota(&config);
    std::string msg;
    if (rv == ESP_OK) {
        msg = "Firmware update in process";
    } else if (rv == ESP_ERR_OTA_VALIDATE_FAILED) {
        msg = "Image Validation Failed";
    } else {
        msg = esp_err_to_name(rv);
    }
    ESP_LOGI(TAG, "Update result: %s", msg.c_str());

    std::string resp = R"(
<html>
  <head>
    <meta http-equiv="refresh" content="5" />
    <title>OTA Update</title>
  </head>
  <body>
)";
    resp.append(msg);

    resp.append(R"(
  </body>
</html>
)");

    req.sendResponse(resp);
    if (rv == ESP_OK) {
        auto t = ticks(1s);
        ESP_LOGI(TAG, "Pending restart in %" PRIu32 " ticks", t);
        restart = xTimerCreate("OTA Restart", t, pdFALSE, nullptr, delayedRestart);
        if (!restart) {
            ESP_LOGI(TAG, "Failed to initialized restart timer!");
        } else {
            xTimerStart(restart, 0);
        }
    }

    return ESP_OK;
}

