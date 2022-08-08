#pragma once

#include <string>
#include "nvs_flash.h"

class NvsFlash {
public:
    NvsFlash(const std::string& ns, nvs_open_mode_t openMode = NVS_READONLY);
    ~NvsFlash();

    esp_err_t getString(const std::string& key, std::string& value);
    esp_err_t setString(const std::string& key, const std::string& value);
    esp_err_t getU16(const std::string& key, uint16_t& value);
    esp_err_t setU16(const std::string& key, const uint16_t value);

    esp_err_t eraseKey(const std::string& key);

    bool dirty() const { return dirty_; }
    esp_err_t commit();

    static void init();

private:
    nvs_handle_t handle_;
    bool dirty_{false};
};

