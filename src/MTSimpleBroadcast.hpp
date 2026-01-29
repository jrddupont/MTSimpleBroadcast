#ifndef MESHTASTIC_SIMPLE_BROADCAST_H
#define MESHTASTIC_SIMPLE_BROADCAST_H

#include "util/MTHelpers.hpp"
#include "util/MathHelpers.hpp"

#include "RadioLib.h"

#include "protobuf/meshtastic/mesh.pb.h"
#include "util/Structs.hpp"
#include <AES.h>
#include <CTR.h>
#include <Crypto.h>

class MTSimpleBroadcast {
private:
    // Radio Settings
    uint8_t send_hop_limit = 7;
    bool ok_to_mqtt = true;
    PhysicalLayer *radio; // SX1262 radio = new Module(hal, 8, 14, 12, 13);

    // Per-boot packet id seed and message counter to avoid duplicate packet IDs
    // across reboots (e.g., when millis() is the same immediately after boot).
    uint32_t boot_packet_seed = 0;
    uint16_t boot_msg_counter = 0;

    MTSB_MyNodeInfo my_nodeinfo; // My node info. Used in many places. Set it carefully.

    void send_now(MTSB_OutQueueEntry entry);

    // Crypto
    CTR<AES128> ctr128;
    CTR<AES192> ctr192;
    CTR<AES256> ctr256;

public:
    MTSimpleBroadcast();

    bool RadioInit(SPIClass &spi, int csPin, int irqPin, int rstPin, int busyPin, LoraConfig &lora_config); // Initializes the radio with the given configuration and pins
    void broadcastNodeInfo(MTSB_NodeInfo &nodeinfo, const MTSB_ChannelEntry &channel);
    void broadcastMyNodeInfo(const MTSB_ChannelEntry &channel) {
        broadcastNodeInfo(my_nodeinfo, channel);
    }
    void broadcastTextMessage(const std::string &text, const MTSB_ChannelEntry &channel);
    bool pb_decode_from_bytes(const uint8_t *srcbuf, size_t srcbufsize, const pb_msgdesc_t *fields, void *dest_struct);                                                                      // decode the protobuf message from bytes
    size_t pb_encode_to_bytes(uint8_t *destbuf, size_t destbufsize, const pb_msgdesc_t *fields, const void *src_struct);                                                                     // encode the protobuf message to bytes
    bool aes_decrypt_meshtastic_payload(const uint8_t *key, uint16_t keySizeBytes, uint32_t packet_id, uint32_t from_node, const uint8_t *encrypted_in, uint8_t *decrypted_out, size_t len); // decrypts the meshtastic payload using AES

    /**
     * @brief Set the My Names object
     *
     * @param short_name
     * @param long_name
     */
    void setMyNames(const char *short_name, const char *long_name);

    /**
     * @brief Set the Send Hop Limit object
     *
     * @param limit
     * @return true Valid hop limit set
     * @return false Invalid hop limit, must be between 1 and 7
     */
    bool setSendHopLimit(uint8_t limit) {
        if (limit > 0 && limit <= 7) {
            send_hop_limit = limit;
            return true;
        }
        return false;
    }

    /**
     * @brief Set the MQTT flag
     *
     * @param ok
     * @return true Allows the message to be sent to the MQTT broker
     * @return false Prevents the message from being sent to the MQTT broker
     */
    void setOkToMqtt(bool ok) {
        ok_to_mqtt = ok;
    }

    // Radio settings on the fly
    bool setRadioFrequency(float freq);
    bool setRadioSpreadingFactor(uint8_t sf);
    bool setRadioBandwidth(uint32_t bw);
    bool setRadioCodingRate(uint8_t cr);
    bool setRadioPower(int8_t power);

    MTSB_MyNodeInfo *getMyNodeInfo() {
        return &my_nodeinfo;
    }
};

#endif // MESHTASTIC_SIMPLE_BROADCAST_H