#include "MTSimpleBroadcast.hpp"
#include "protobuf/meshtastic/remote_hardware.pb.h"
#include "protobuf/meshtastic/telemetry.pb.h"
#include "protobuf/pb.h"
#include "protobuf/pb_decode.h"
#include "protobuf/pb_encode.h"

#define PACKET_FLAGS_HOP_LIMIT_MASK 0x07
#define PACKET_FLAGS_WANT_ACK_MASK 0x08
#define PACKET_FLAGS_VIA_MQTT_MASK 0x10
#define PACKET_FLAGS_HOP_START_MASK 0xE0
#define PACKET_FLAGS_HOP_START_SHIFT 5

#define BITFIELD_WANT_RESPONSE_SHIFT 1
#define BITFIELD_OK_TO_MQTT_SHIFT 0
#define BITFIELD_WANT_RESPONSE_MASK (1 << BITFIELD_WANT_RESPONSE_SHIFT)
#define BITFIELD_OK_TO_MQTT_MASK (1 << BITFIELD_OK_TO_MQTT_SHIFT)
#define NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_SHIFT 0
#define NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_MASK (1 << NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_SHIFT)

#pragma region Radio Initialization
MTSimpleBroadcast::MTSimpleBroadcast() {
    uint64_t mac = 1337;
    // esp_efuse_mac_get_default((uint8_t *)&mac); // Use the last 4 bytes of the MAC address as the node ID
    my_nodeinfo.node_id = (uint32_t)(mac & 0xFFFFFFFF);
    sprintf(my_nodeinfo.id, "!0x%08" PRIx32, my_nodeinfo.node_id);
    my_nodeinfo.hw_model = (uint8_t)meshtastic_HardwareModel_DIY_V1;
    my_nodeinfo.role = (uint8_t)meshtastic_Config_DeviceConfig_Role_CLIENT;

    my_nodeinfo.macaddr[0] = (uint8_t)(mac >> 40);
    my_nodeinfo.macaddr[1] = (uint8_t)(mac >> 32);
    my_nodeinfo.macaddr[2] = (uint8_t)(mac >> 24);
    my_nodeinfo.macaddr[3] = (uint8_t)(mac >> 16);
    my_nodeinfo.macaddr[4] = (uint8_t)(mac >> 8);
    my_nodeinfo.macaddr[5] = (uint8_t)(mac & 0xFF);

    my_nodeinfo.public_key_size = 0;                                   // Set to 0 if no public key is available
    memset(my_nodeinfo.public_key, 0, sizeof(my_nodeinfo.public_key)); // Initialize public key to zero
    sprintf(my_nodeinfo.short_name, "MCP");
    snprintf(my_nodeinfo.long_name, sizeof(my_nodeinfo.long_name) - 1, "MtCompact-%02" PRIx32, my_nodeinfo.node_id);

    // initialize per-boot packet seed/counter so packet IDs are unique across reboots
    boot_packet_seed = ((uint32_t)random() << 16) | (uint32_t)random(0, 65536);
    if (boot_packet_seed == 0)
        boot_packet_seed = 1; // avoid zero seed
    boot_msg_counter = 0;
}

// Only support SX1262 for now
bool MTSimpleBroadcast::RadioInit(SPIClass &spi, int csPin, int irqPin, int rstPin, int busyPin, LoraConfig &lora_config) {
    Serial.println("RadioInit");
    int state = RADIOLIB_ERR_NONE;
    Serial.println("Using SX1262 radio");
    radio = new SX1262(new Module(csPin, irqPin, rstPin, busyPin, spi));
    state = ((SX1262 *)radio)->begin(lora_config.frequency, lora_config.bandwidth, lora_config.spreading_factor, lora_config.coding_rate, lora_config.sync_word, lora_config.output_power, lora_config.preamble_length, lora_config.tcxo_voltage, lora_config.use_regulator_ldo);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("failed, code " + String(state));
        delete radio;
        radio = nullptr;
        return false;
    }

    // todo do it less ugly.
    state |= ((SX1262 *)radio)->setCurrentLimit(130.0);
    state |= ((SX1262 *)radio)->explicitHeader();
    state |= ((SX1262 *)radio)->setCRC(RADIOLIB_SX126X_LORA_CRC_ON);
    state |= ((SX1262 *)radio)->setDio2AsRfSwitch(false);
    // ((SX1262 *)radio)->setDio1Action(onPacketReceived);
    state |= ((SX1262 *)radio)->setRxBoostedGainMode(true);

    if (state != 0) {
        Serial.println("Radio init failed, code " + String(state));
        return false;
    }

    return true;
}

bool MTSimpleBroadcast::setRadioFrequency(float freq) {
    if (radio == nullptr) {
        return false;
    }
    int state = RADIOLIB_ERR_NONE;
    state = radio->setFrequency(freq);
    return (state == RADIOLIB_ERR_NONE);
}

bool MTSimpleBroadcast::setRadioSpreadingFactor(uint8_t sf) {
    if (radio == nullptr) {
        return false;
    }
    int state = RADIOLIB_ERR_NONE;
    state = ((SX126x *)radio)->setSpreadingFactor(sf);
    return (state == RADIOLIB_ERR_NONE);
}

bool MTSimpleBroadcast::setRadioBandwidth(uint32_t bw) {
    if (radio == nullptr) {
        return false;
    }
    int state = RADIOLIB_ERR_NONE;
    state = ((SX126x *)radio)->setBandwidth(bw);
    return (state == RADIOLIB_ERR_NONE);
}

bool MTSimpleBroadcast::setRadioCodingRate(uint8_t cr) {
    if (radio == nullptr) {
        return false;
    }
    int state = RADIOLIB_ERR_NONE;
    state = ((SX126x *)radio)->setCodingRate(cr);
    return (state == RADIOLIB_ERR_NONE);
}

bool MTSimpleBroadcast::setRadioPower(int8_t power) {
    if (radio == nullptr) {
        return false;
    }
    int state = RADIOLIB_ERR_NONE;
    state = radio->setOutputPower(power);
    return (state == RADIOLIB_ERR_NONE);
}

#pragma region Send Packets

void MTSimpleBroadcast::send_now(MTSB_OutQueueEntry entry) {

    // Create a unique packet ID if not set
    if (entry.header.packet_id == 0) {
        uint32_t millis_val = (uint32_t)millis();
        uint32_t counter = ++(boot_msg_counter);
        uint32_t seed = boot_packet_seed;

        // Combine inputs
        uint32_t x = seed;
        x ^= millis_val + 0x9E3779B9u;      // golden ratio offset
        x ^= counter + (x << 6) + (x >> 2); // Jenkins-style mix

        // Strong avalanche (finalizer)
        x ^= x >> 16;
        x *= 0x85EBCA6Bu;
        x ^= x >> 13;
        x *= 0xC2B2AE35u;
        x ^= x >> 16;

        entry.header.packet_id = x;
    }

    // Prepare the payload
    uint8_t payload[256];
    uint8_t relay_node = 0;

    if (ok_to_mqtt) {
        entry.data.bitfield |= 1 << BITFIELD_OK_TO_MQTT_SHIFT; // Set the MQTT upload bit
    }
    entry.data.bitfield |= 1 << (entry.data.want_response << BITFIELD_WANT_RESPONSE_SHIFT);
    entry.data.has_bitfield = true;

    size_t payload_len = pb_encode_to_bytes(payload, sizeof(payload), meshtastic_Data_fields, &entry.data);

    // Encrypt the payload if needed
    uint8_t encrypted_payload[256];

    if (aes_decrypt_meshtastic_payload(entry.key, entry.key_len, entry.header.packet_id, entry.header.srcnode, payload, encrypted_payload, payload_len)) {
    } else {
        Serial.println("Failed to encrypt payload");
        return;
    }

    // here we got the encrypted payload
    // create header. ugly but we'll reuse the payload buffer
    memcpy(&payload[0], &entry.header.dstnode, sizeof(uint32_t));
    memcpy(&payload[4], &entry.header.srcnode, sizeof(uint32_t));
    memcpy(&payload[8], &entry.header.packet_id, sizeof(uint32_t));

    payload[12] = (entry.header.hop_limit & PACKET_FLAGS_HOP_LIMIT_MASK) | (entry.header.want_ack ? PACKET_FLAGS_WANT_ACK_MASK : 0) | (entry.header.via_mqtt ? PACKET_FLAGS_VIA_MQTT_MASK : 0) | ((entry.header.hop_start & 0x07) << PACKET_FLAGS_HOP_START_SHIFT);
    payload[13] = entry.header.chan_hash;
    payload[14] = 0;          // entry.header.packet_next_hop; -- no preference
    payload[15] = relay_node; // entry.header.packet_relay_node;

    // copy the encrypted payload to the end of the header
    size_t total_len = 16 + payload_len; // 16 bytes for header + payload length

    if (total_len > sizeof(payload)) {
        Serial.println("Payload too large: " + String(total_len));
        return;
    }
    memcpy(&payload[16], encrypted_payload, payload_len);

    // Send the packet
    while (radio->scanChannel() != RADIOLIB_CHANNEL_FREE) {
        // channel busy, wait a bit
        delay(20);
    }

    Serial.println("Sending packet...");
    int err = radio->transmit(payload, total_len);

    if (err == RADIOLIB_ERR_NONE) {
        Serial.println("Packet sent successfully");
    } else {
        Serial.println("Failed to send packet, code " + String(err));
        delay(30);

        Serial.println("Sending packet again...");
        err = radio->transmit(payload, total_len);

        if (err == RADIOLIB_ERR_NONE) {
            Serial.println("Packet sent successfully in 2nd try ");
        } else {
            Serial.println("Failed to send packet 2 times in a row, code " + String(err));
        }
    }
}

#pragma endregion

#pragma region Packet Builders

void MTSimpleBroadcast::broadcastNodeInfo(MTSB_NodeInfo &nodeinfo, const MTSB_ChannelEntry &channel) {
    MTSB_OutQueueEntry entry;
    entry.header.dstnode = 0xffffffff; // broadcast
    entry.header.srcnode = nodeinfo.node_id;
    entry.header.packet_id = 0;
    entry.header.hop_limit = send_hop_limit;
    entry.header.want_ack = 0;
    entry.header.via_mqtt = false;
    entry.header.hop_start = send_hop_limit;
    entry.header.chan_hash = channel.hash[0];

    entry.header.via_mqtt = 0; // Not used in this case
    entry.encType = 1;         // AES encryption
    meshtastic_User user_msg = {};
    memcpy(user_msg.id, nodeinfo.id, sizeof(user_msg.id));
    memcpy(user_msg.short_name, nodeinfo.short_name, sizeof(user_msg.short_name));
    memcpy(user_msg.long_name, nodeinfo.long_name, sizeof(user_msg.long_name));
    memcpy(user_msg.macaddr, nodeinfo.macaddr, sizeof(user_msg.macaddr));
    memcpy(user_msg.public_key.bytes, nodeinfo.public_key, sizeof(user_msg.public_key.bytes));

    user_msg.public_key.size = nodeinfo.public_key_size;
    user_msg.is_licensed = false;
    entry.data.bitfield = 0;
    if (ok_to_mqtt) {
        entry.data.bitfield |= 1 << BITFIELD_OK_TO_MQTT_SHIFT; // Set the MQTT upload bit
    }

    entry.data.has_bitfield = true;
    bool all_zero = true;
    for (size_t i = 0; i < sizeof(user_msg.public_key.bytes); i++) {
        if (user_msg.public_key.bytes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        user_msg.public_key.size = 0; // Set size to 0 if all bytes are zero //todo maybe delete
    }

    user_msg.role = (meshtastic_Config_DeviceConfig_Role)nodeinfo.role;
    user_msg.hw_model = (meshtastic_HardwareModel)nodeinfo.hw_model;
    user_msg.is_licensed = false;
    user_msg.is_unmessagable = false;
    entry.data.portnum = meshtastic_PortNum_NODEINFO_APP;
    entry.data.request_id = 0;
    entry.data.reply_id = 0;
    entry.data.want_response = false;
    entry.data.payload.size = pb_encode_to_bytes((uint8_t *)&entry.data.payload.bytes, sizeof(entry.data.payload.bytes), &meshtastic_User_msg, &user_msg);
    // Use provided channel secret for encryption
    entry.key = (uint8_t *)channel.secret;
    entry.key_len = channel.secret_len;

    send_now(entry);
}

void MTSimpleBroadcast::broadcastTextMessage(const std::string &text, const MTSB_ChannelEntry &channel) {
    MTSB_OutQueueEntry entry;
    entry.header.dstnode = 0xffffffff; // broadcast
    entry.header.srcnode = my_nodeinfo.node_id;
    entry.header.packet_id = 0;
    entry.header.hop_limit = send_hop_limit;
    entry.header.want_ack = 0;
    entry.header.via_mqtt = false;
    entry.header.hop_start = send_hop_limit;
    // Use channel's precomputed 1-byte hash; private messages use chan_hash==0
    entry.header.chan_hash = channel.hash[0];
    entry.header.via_mqtt = 0;
    entry.encType = 1; // AES encryption
    entry.data.request_id = 0;
    entry.data.reply_id = 0;
    entry.data.payload.size = text.size();
    memcpy(entry.data.payload.bytes, text.data(), text.size());
    entry.data.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP; // NodeInfo portnum
    entry.data.want_response = 0;
    entry.data.emoji = 0;

    entry.data.bitfield = 0;
    if (ok_to_mqtt) {
        entry.data.bitfield |= 1 << BITFIELD_OK_TO_MQTT_SHIFT; // Set the MQTT upload bit
    }
    entry.data.has_bitfield = true;

    // Use provided channel secret for encryption
    entry.key = (uint8_t *)channel.secret;
    entry.key_len = channel.secret_len;

    send_now(entry);
}

#pragma endregion

#pragma region Encode Decode
bool MTSimpleBroadcast::aes_decrypt_meshtastic_payload(const uint8_t *key, uint16_t keySizeBytes, uint32_t packet_id, uint32_t from_node, const uint8_t *encrypted_in, uint8_t *decrypted_out, size_t len) {

    // keySizeBits must be 128, 192, or 256
    if (keySizeBytes != 16 && keySizeBytes != 24 && keySizeBytes != 32) {
        Serial.println("Invalid AES key size");
        return false;
    }

    // Prepare the nonce (16 bytes)
    uint8_t nonce[16] = {0};
    memcpy(nonce, &packet_id, sizeof(uint32_t));
    memcpy(nonce + 8, &from_node, sizeof(uint32_t));

    // Switch based on key size
    switch (keySizeBytes) {
    case 16: // AES128
        if (!ctr128.setKey(key, 16)) {
            Serial.println("Failed to set AES128 key");
            return false;
        }
        ctr128.setIV(nonce, sizeof(nonce));
        ctr128.decrypt(decrypted_out, encrypted_in, len);
        break;

    case 24: // AES192
        if (!ctr192.setKey(key, 24)) {
            Serial.println("Failed to set AES192 key");
            return false;
        }
        ctr192.setIV(nonce, sizeof(nonce));
        ctr192.decrypt(decrypted_out, encrypted_in, len);
        break;

    case 32: // AES256
        if (!ctr256.setKey(key, 32)) {
            Serial.println("Failed to set AES256 key");
            return false;
        }
        ctr256.setIV(nonce, sizeof(nonce));
        ctr256.decrypt(decrypted_out, encrypted_in, len);
        break;

    default:
        // Should never reach here
        return false;
    }

    return true;
}

size_t MTSimpleBroadcast::pb_encode_to_bytes(uint8_t *destbuf, size_t destbufsize, const pb_msgdesc_t *fields, const void *src_struct) {
    pb_ostream_t stream = pb_ostream_from_buffer(destbuf, destbufsize);
    if (!pb_encode(&stream, fields, src_struct)) {
        return 0;
    } else {
        return stream.bytes_written;
    }
}

// Not needed since this package only sends messages
// bool MTSimpleBroadcast::pb_decode_from_bytes(const uint8_t *srcbuf, size_t srcbufsize, const pb_msgdesc_t *fields, void *dest_struct) {
//     pb_istream_t stream = pb_istream_from_buffer(srcbuf, srcbufsize);
//     if (!pb_decode(&stream, fields, dest_struct)) {
//         return false;
//     } else {
//         return true;
//     }
// }
#pragma endregion