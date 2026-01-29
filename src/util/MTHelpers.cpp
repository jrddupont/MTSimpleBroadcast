#include "MTHelpers.hpp"
#include "protobuf/meshtastic/mesh.pb.h"

void MTHelpers::NodeInfoBuilder(MTSB_NodeInfo *nodeinfo, uint32_t node_id, std::string &short_name, std::string &long_name, uint8_t hw_model) {
    nodeinfo->node_id = node_id;
    if (long_name.empty()) {
        char hex_part[7];
        snprintf(hex_part, sizeof(hex_part), "%06" PRIx32, (node_id & 0xFFFFFF));
        hex_part[sizeof(hex_part) - 1] = '\0';
        std::string generated_name = "Meshtastic-" + std::string(hex_part);
        long_name = generated_name;
    }
    if (short_name.empty()) {
        char hex_part[5];
        snprintf(hex_part, sizeof(hex_part), "%04" PRIx32, node_id & 0xFFFF);
        hex_part[sizeof(hex_part) - 1] = '\0';
        short_name = std::string(hex_part);
    }

    strncpy(nodeinfo->short_name, short_name.c_str(), sizeof(nodeinfo->short_name) - 1);
    nodeinfo->short_name[sizeof(nodeinfo->short_name) - 1] = '\0';
    strncpy(nodeinfo->long_name, long_name.c_str(), sizeof(nodeinfo->long_name) - 1);
    nodeinfo->long_name[sizeof(nodeinfo->long_name) - 1] = '\0';
    nodeinfo->hw_model = (uint8_t)meshtastic_HardwareModel_DIY_V1;
    snprintf(nodeinfo->id, sizeof(nodeinfo->id), "!%08" PRIx32, node_id);
    nodeinfo->id[sizeof(nodeinfo->id) - 1] = '0';
    nodeinfo->role = 0;
    for (int i = 0; i < 6; ++i) {
        nodeinfo->macaddr[i] = (node_id >> (8 * (5 - i))) & 0xFF;
    }
    memset(nodeinfo->public_key, 0, sizeof(nodeinfo->public_key));
    nodeinfo->public_key_size = 0; // Set to 0 if no public key is available
    nodeinfo->hw_model = hw_model; // Set hardware model
}
