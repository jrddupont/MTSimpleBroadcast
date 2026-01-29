#include "MathHelpers.hpp"
#include "base64.hpp"

uint8_t MathHelpers::xorHash(const uint8_t *data, size_t len) {
    uint8_t code = 0;
    for (size_t i = 0; i < len; i++)
        code ^= data[i];
    return code;
}

// Default Meshtastic base key
// "AQ=="
const std::uint8_t defaultKey[16] = {
    0xd4, 0xf1, 0xbb, 0x3a,
    0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab,
    0xcf, 0x4e, 0x69, 0x01};

// Converts a channel key string to a 16-byte key
// Supports 4-character (1 byte) and 24-character (16 byte) base64-encoded keys
bool MathHelpers::channelKeyTo16Bytes(const std::string &input, std::uint8_t *buffer, size_t bufferSize) {
    if (bufferSize < 16) {
        return false; // buffer too small
    }

    // Copy default key into provided buffer
    for (int i = 0; i < 16; ++i) {
        buffer[i] = defaultKey[i];
    }

    if (input.size() == 4) {
        // Meshtastic treats one byte keys specially by placing the byte at the end
        // of a predefined 16-byte key. The default channel key "AQ==" decodes to 0x01.
        std::uint8_t decoded;
        int bytes = decode_base64(reinterpret_cast<const unsigned char *>(input.data()), input.size(), &decoded);
        if (bytes != 1)
            return false;
        buffer[15] = decoded;
    } else if (input.size() == 24) {
        // 16-byte keys are directly decoded into the buffer
        int bytes = decode_base64(reinterpret_cast<const unsigned char *>(input.data()), input.size(), buffer);
        if (bytes != 16)
            return false;
    } else {
        return false;
    }

    return true;
}

// Converts a channel key string to a 32-byte key
// Supports only 44-character (32 byte) base64-encoded keys
bool MathHelpers::channelKeyTo32Bytes(const std::string &input, std::uint8_t *buffer, size_t bufferSize) {
    if (bufferSize < 32) {
        return false; // buffer too small
    }
    if (input.size() != 44) {
        return false; // invalid input size
    }

    int bytes = decode_base64(reinterpret_cast<const unsigned char *>(input.data()), input.size(), buffer);
    if (bytes != 32) {
        return false;
    } else {
        return true;
    }
}