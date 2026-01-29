#pragma once
#include "AES.h"
#include "Structs.hpp"
#include <Crypto.h>
#include <Curve25519.h>
#include <RNG.h>
#include <inttypes.h>
#include <string>

#define CryptRNG RNG

/**
 * @brief Simple static helpers for building message objects easily.
 *
 */
class MTHelpers {
public:
    static void NodeInfoBuilder(MTSB_NodeInfo *nodeinfo, uint32_t node_id, std::string &short_name, std::string &long_name, uint8_t hw_model);
};
