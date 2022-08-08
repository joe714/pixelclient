#include "NvsFlash.h"

void
NvsFlash::init()
{
    esp_err_t rv = nvs_flash_init();
    if (rv == ESP_ERR_NVS_NO_FREE_PAGES ||
        rv == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        rv = nvs_flash_init();
    }
    ESP_ERROR_CHECK(rv);
}

NvsFlash::NvsFlash(const std::string& ns, nvs_open_mode_t openMode)
{
    esp_err_t rv = nvs_open(ns.c_str(), openMode, &handle_);
    ESP_ERROR_CHECK(rv);
}

NvsFlash::~NvsFlash() { nvs_close(handle_); }

esp_err_t
NvsFlash::setString(const std::string& key, const std::string& value)
{
    esp_err_t rv = nvs_set_str(handle_, key.c_str(), value.c_str());
    if (rv == ESP_OK) {
        dirty_ = true;
    }

    return rv;
}

esp_err_t
NvsFlash::getString(const std::string& key, std::string& value)
{
    size_t len = 0;
    esp_err_t rv = nvs_get_str(handle_, key.c_str(), nullptr, &len);
    if (ESP_OK != rv && ESP_ERR_NVS_INVALID_LENGTH != rv) {
        return rv;
    }
    value.resize(len - 1);
    rv = nvs_get_str(handle_, key.c_str(), value.data(), &len);
    return rv;
}

esp_err_t
NvsFlash::getU16(const std::string& key, uint16_t& value)
{
    esp_err_t rv = nvs_get_u16(handle_, key.c_str(), &value);
    return rv;
}

esp_err_t
NvsFlash::setU16(const std::string& key, uint16_t value)
{
    esp_err_t rv = nvs_set_u16(handle_, key.c_str(), value);
    if (ESP_OK == rv) {
        dirty_ = true;
    }
    return rv;
}

esp_err_t
NvsFlash::eraseKey(const std::string& key)
{
    esp_err_t rv = nvs_erase_key(handle_, key.c_str());
    if (ESP_OK == rv) {
        dirty_ = true;
    }
    return rv;
}

esp_err_t
NvsFlash::commit()
{
    if (!dirty_) {
        return ESP_OK;
    }

    esp_err_t rv = nvs_commit(handle_);
    if (ESP_OK == rv) {
        dirty_ = false;
    }
    return rv;
}
