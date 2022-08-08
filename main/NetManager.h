#pragma once
#include <stdint.h>

class NetManager {
public:
    NetManager();

    void start();

    const uint8_t* stationMac() const { return stationMac_; }
private:
    uint8_t stationMac_[6];
};
