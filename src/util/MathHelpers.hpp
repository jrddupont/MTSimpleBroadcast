#pragma once

#include <stdint.h>
#include <string>

class MathHelpers {
public:
    static uint8_t xorHash(const uint8_t *data, size_t len);
    static bool channelKeyTo16Bytes(const std::string &input, uint8_t *buffer, size_t bufferSize);
    static bool channelKeyTo32Bytes(const std::string &input, uint8_t *buffer, size_t bufferSize);
};