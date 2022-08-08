#pragma once
#include <string>

class Config
{
public:
    static Config& instance();

    std::string imageUri() const;
    const std::string& deviceUUID() const { return deviceUUID_; }
    const std::string& endpoint() const { return endpoint_; }

private:
    Config();

    std::string endpoint_;
    uint16_t port_;
    std::string deviceUUID_;
};
