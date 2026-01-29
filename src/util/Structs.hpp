#pragma once

#include "protobuf/meshtastic/mesh.pb.h"
#include "util/MathHelpers.hpp"
#include <stdint.h>
#include <string>

class LoraConfig {
public:
    LoraConfig() {};
    LoraConfig(float freq, float bw, uint8_t sf, uint8_t cr, uint8_t sw, uint16_t pl, int8_t op, float tv, bool ldo)
        : frequency(freq),
          bandwidth(bw),
          spreading_factor(sf),
          coding_rate(cr),
          sync_word(sw),
          preamble_length(pl),
          output_power(op),
          tcxo_voltage(tv),
          use_regulator_ldo(ldo) {};
    LoraConfig(float freq, int8_t output_power = 22, float tcxo_voltage = 1.8, bool use_regulator_ldo = false)
        : frequency(freq),
          output_power(output_power),
          tcxo_voltage(tcxo_voltage),
          use_regulator_ldo(use_regulator_ldo) {};

    float frequency = 869.525;      // Frequency in MHz
    float bandwidth = 250.0;        // Bandwidth in kHz
    uint8_t spreading_factor = 8;   // Spreading factor (7-12)
    uint8_t coding_rate = 11;       // Coding rate denominator (5-8)
    uint8_t sync_word = 0x2b;       // Sync word
    uint16_t preamble_length = 16;  // Preamble length in symbols
    int8_t output_power = 22;       // Output power in dBm
    float tcxo_voltage = 1.8;       // TCXO voltage in volts
    bool use_regulator_ldo = false; // Use LDO regulator (true) or DC-DC regulator (false)
};

class MTSB_ChannelEntry {
public:
    MTSB_ChannelEntry() {};

    // Build from name and secret bytes
    MTSB_ChannelEntry(std::string n, uint8_t s[32], size_t len) : name(n), secret_len(len) {
        memcpy(secret, s, len);
        calcHash();
    };

    // Build from channel key string
    MTSB_ChannelEntry(const std::string &n, const std::string &channelKey) : name(n) {
        // Determine secret_len based on channelKey length
        size_t keyLen = channelKey.length();
        if (keyLen == 4 || keyLen == 24) {
            secret_len = 16;
        } else if (keyLen == 44) {
            secret_len = 32;
        } else {
            // Default fallback if length doesn't match known patterns
            secret_len = -1;
        }

        memset(secret, 0, sizeof(secret));

        if (secret_len == 16) {
            bool result = MathHelpers::channelKeyTo16Bytes(channelKey, secret, secret_len);
        } else if (secret_len == 32) {
            bool result = MathHelpers::channelKeyTo32Bytes(channelKey, secret, secret_len);
        }

        calcHash();
    }

    void calcHash() {
        hash[0] = MathHelpers::xorHash((const uint8_t *)name.c_str(), name.length());
        hash[0] ^= MathHelpers::xorHash(secret, secret_len);
    }

    std::string name;
    uint8_t secret[32];
    size_t secret_len;
    uint8_t hash[1];
};

class MTSB_NodeInfo {
public:
    uint32_t node_id;        // src
    char id[16];             // Node ID
    char short_name[5];      // Short name of the node
    char long_name[40];      // Long name of the node
    uint8_t hw_model;        // Hardware model
    uint8_t macaddr[6];      // MAC address (not used in this struct)
    uint8_t public_key[32];  // Public key (not used in this struct)
    uint8_t public_key_size; // Size of the public key
    uint8_t role;            // Role of the node
    uint32_t last_updated;   // Last updated timestamp
};

class MTSB_MyNodeInfo : public MTSB_NodeInfo {
public:
    uint8_t private_key[32];
};

struct MTSB_Header {
    uint32_t srcnode; // source node ID
    uint32_t dstnode; // destination node ID
    uint32_t packet_id;
    uint8_t hop_limit;
    uint8_t hop_start;
    uint8_t chan_hash;
    bool want_ack; // true if the sender wants an acknowledgment
    bool via_mqtt; // true if the packet is sent via MQTT
    float rssi;
    float snr;
    uint32_t request_id;
    uint32_t reply_id;
};

struct MTSB_OutQueueEntry {
    MTSB_Header header;
    meshtastic_Data data;
    uint8_t encType = 0; // 0 = auto, 1 = aes, 2 = key
    uint8_t *key;
    size_t key_len; // Length of the key in bytes
};