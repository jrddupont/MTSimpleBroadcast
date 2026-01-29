# MT Simple Broadcast
This is a slimmed down version of [EspMeshCompact](https://github.com/htotoo/EspMeshCompact) designed to provide simple broadcast functionality for Meshtastic.   
It is very experimental and should be used with caution.

## Example Usage
Running on a [Raspberry Pi Pico 1](https://www.raspberrypi.com/products/raspberry-pi-pico/) with a [Waveshare Core1262](https://www.amazon.com/dp/B09LV2W64R)  

Requires these libraries:
* jgromes/RadioLib
* rweather/Crypto
* densaugeo/base64

```cpp
#include <Arduino.h>
#include <MTSimpleBroadcast.hpp>

// SPI
#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_MISO 12
#define PIN_CS 4

// LoRa
#define PIN_IRQ 20 // DIO1
#define PIN_BUSY 2
#define PIN_RST 3

// MediumFast Meshtastic Config
LoraConfig lora_config_mt = {
    /*.frequency = */ 913.125,  // config
    /*.bandwidth = */ 250,      // config
    /*.spreading_factor = */ 9, // config
    /*.coding_rate = */ 5,      // config
    /*.sync_word = */ 0x2B,
    /*.preamble_length = */ 16,
    /*.output_power = */ 22, // config
    /*.tcxo_voltage = */ 1.8,
    /*.use_regulator_ldo = */ false,
}; //

MTSimpleBroadcast mtsb;

void setup() {

    Serial.begin(115200);
    while (!Serial) { }
    Serial.println("Serial Active");

    Serial.println("Initializing SPI...");
    SPI1.setSCK(PIN_SCK);
    SPI1.setTX(PIN_MOSI);
    SPI1.setRX(PIN_MISO);
    SPI1.begin();

    // Make sure that this pin is unconnected and floating
    randomSeed(analogRead(A0));

    Serial.println("Starting radio...");
    mtsb.RadioInit(SPI1, PIN_CS, PIN_IRQ, PIN_RST, PIN_BUSY, lora_config_mt);
    Serial.println("Radio started.");

    Serial.println("Configuring radio...");
    mtsb.setSendHopLimit(2);

    uint32_t node_id = 0xABBABAAB;
    std::string short_name = "rppt";
    std::string long_name = "pi_pico_test";
    uint8_t hardware_model = 44;

    MTHelpers::NodeInfoBuilder(mtsb.getMyNodeInfo(), node_id, short_name, long_name, hardware_model);
    Serial.println("Radio configured.");

    MTSB_ChannelEntry channel("MediumFast", "AQ==");

    Serial.println("Sending test message...");
    std::string test_msg = "Hello World!";
    mtsb.broadcastMyNodeInfo(channel);
    delay(500);
    mtsb.broadcastTextMessage(test_msg, channel);
    Serial.println("Test message sent.");
}

void loop() {
    // put your main code here, to run repeatedly:
}
```