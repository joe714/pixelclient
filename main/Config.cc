#include "Config.h"

#include "esp_log.h"
#include "esp_random.h"

#include "NvsFlash.h"

static const char* TAG = "CONFIG";

namespace {
std::string generateClientId()
{
    // Make a v4 UUID per RFC 4122 section 4.4
    static int KEYLEN = 16;
    static const char ENC[] = "0123456789abcdef";
    uint8_t key[KEYLEN];
    esp_fill_random(key, KEYLEN);
    key[8] = (key[8] & 0x3f) | 0x80;
    key[6] = (key[6] & 0x0f) | 0x40;

    std::string out;
    for (int i = 0; i < KEYLEN; ++i) {
        out.push_back(ENC[(key[i] & 0xf0) >> 4]);
        out.push_back(ENC[key[i] & 0x0f]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            out.push_back('-');
        }
    }
    ESP_LOGD(TAG, "Generated device UUID %s", out.c_str());
    return out;
}
} // namespace

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

Config::Config()
{
    NvsFlash flash("PixelMatrix", NVS_READWRITE);

    esp_err_t rv = flash.getString("endpoint", endpoint_);
    if (ESP_ERR_NVS_NOT_FOUND == rv) {
        endpoint_ = "192.168.4.59";
    }

    rv = flash.getU16("port", port_);
    if (ESP_ERR_NVS_NOT_FOUND == rv) {
        port_ = 8080;
    }

    rv = flash.getString("deviceUUID", deviceUUID_);
    if (ESP_ERR_NVS_NOT_FOUND == rv) {
        // Legacy NVS
        rv = flash.getString("clientId", deviceUUID_);
        if (ESP_OK == rv) {
            (void)flash.setString("deviceUUID", deviceUUID_);
            (void)flash.eraseKey("clientId");
        }
    }

    if (ESP_ERR_NVS_NOT_FOUND == rv || deviceUUID_.size() != 36) {
        deviceUUID_ = generateClientId();
        rv = flash.setString("deviceUUID", deviceUUID_);
        ESP_ERROR_CHECK(rv);
        rv = flash.commit();
        ESP_ERROR_CHECK(rv);
    }
    ESP_LOGI(TAG, "Device UUID: %s", deviceUUID_.c_str());
}

std::string 
Config::imageUri() const {
    std::string uri = "ws://";
    uri.append(endpoint_)
        .append(":")
        .append(std::to_string(port_))
        .append("/ws?device=")
        .append(deviceUUID_);
    return uri;
}
