# MT Simple Broadcast
This is a slimmed down version of [EspMeshCompact](https://github.com/htotoo/EspMeshCompact) designed to provide simple broadcast functionality for Meshtastic.   
It is very experimental and should be used with caution.

## Example Usage
### Environment
Running on a [Raspberry Pi Pico 1](https://www.raspberrypi.com/products/raspberry-pi-pico/) with a [Waveshare Core1262](https://www.amazon.com/dp/B09LV2W64R)  

Requires these libraries:
* jgromes/RadioLib
* rweather/Crypto
* densaugeo/base64  
  
Built for arduino or earlephilhower's pico-arduino-sdk.  
`platformio.ini`:
```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino
board_build.core = earlephilhower
monitor_speed = 115200
lib_deps = 
	jgromes/RadioLib@^7.5.0
	rweather/Crypto@^0.4.0
	densaugeo/base64@^1.4.0
```

### Code Example
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

    // Set up SPI
    SPI1.setSCK(PIN_SCK);
    SPI1.setTX(PIN_MOSI);
    SPI1.setRX(PIN_MISO);
    SPI1.begin();

    // Initialize Random with a floating pin
    randomSeed(analogRead(A0));

    // Initialize Radio
    mtsb.RadioInit(SPI1, PIN_CS, PIN_IRQ, PIN_RST, PIN_BUSY, lora_config_mt);
    mtsb.setSendHopLimit(2);

    // Configure Node Info
    uint32_t node_id = 0xABBABAAB;
    std::string short_name = "rppt";
    std::string long_name = "pi_pico_test";
    uint8_t hardware_model = 44;
    MTHelpers::NodeInfoBuilder(mtsb.getMyNodeInfo(), node_id, short_name, long_name, hardware_model);

    // Define Channel
    MTSB_ChannelEntry channel("MediumFast", "AQ==");

    // Broadcast Node Info
    mtsb.broadcastMyNodeInfo(channel);
    delay(500);

    // Broadcast Text Message
    std::string test_msg = "Hello World!";
    mtsb.broadcastTextMessage(test_msg, channel);
}

void loop() { }
```

### Wiring
Waveshare Core1262 <-> Raspberry Pi Pico 1

Core1262 Pin      | Pico Pin
------------------|----------------
VCC               | 3V3(OUT)
GND               | GND
MISO              | GPIO12
MOSI              | GPIO11
SCK               | GPIO10
CS                | GPIO4
DIO1 (IRQ)        | GPIO20
RESET             | GPIO3
BUSY              | GPIO2

![Wiring](res/example_diagram.jpg)