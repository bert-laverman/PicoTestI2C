/*
 * Copyright (c) 2024 by Bert Laverman. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <cstring>
#include <memory>

#include <pico/stdlib.h>
#include <pico/unique_id.h>
#include <hardware/gpio.h>

#include <raspberry-pi.hpp>
#include <interfaces/pico-i2c.hpp>
#include <protocols/pico-i2c-protocol-driver.hpp>
#include <protocols/i2c-device-handler.hpp>

#include <components/local-led.hpp>

#if defined(HAVE_MAX7219)
#include <interfaces/pico-spi.hpp>
#include <devices/local-max7219.hpp>
#include <protocols/max7219-handler.hpp>
#endif

using namespace nl::rakis::raspberrypi;
using nl::rakis::raspberrypi::components::Led;


#if !defined(HAVE_I2C)
#error "This example needs I2C enabled"
#endif


static protocols::PicoI2CProtocolDriver driver;

#if defined(TEST_TRIGGER)
// Test build: the first address request is sent exactly when this line goes high, so that several devices that
// share the line ask for an address at the same moment. (Build with -DTEST_TRIGGER=ON.)
static constexpr unsigned TRIGGER_PIN{ 2 };
#endif


/**
 * Pretend to be a device on the bus: wait for the bus controller to say "Hello", ask it for an address
 * with our own board ID, and start listening on the address we get back.
 */
int main([[maybe_unused]] int argc, [[maybe_unused]] const char **argv)
{
    stdio_init_all();

    sleep_ms(5000);
    printf("Starting Pico I2C test\n");

    RaspberryPi& berry{ RaspberryPi::instance() };

    protocols::BoardId myBoardId;
    pico_unique_board_id_t myId;
    pico_get_unique_board_id(&myId);
    for (unsigned i = 0; i < protocols::idSize; i++) { myBoardId.bytes[i] = myId.id[i]; }
    printf("My board ID is %02x%02x%02x%02x-%02x%02x%02x%02x\n",
           myId.id[0], myId.id[1], myId.id[2], myId.id[3], myId.id[4], myId.id[5], myId.id[6], myId.id[7]);

    auto i2c = std::make_shared<interfaces::PicoI2C>(i2c1, 14, 15);
    i2c->name("i2c-1");
    i2c->verbose(false);   // no logging from the I2C interrupt handler

    printf("Initializing I2C driver\n");
    driver.verbose(true);
    driver.addInterface(i2c);

    protocols::I2CDeviceHandler<protocols::PicoI2CProtocolDriver> deviceHandler(driver, myBoardId);
    deviceHandler.registerAsDevice();

#if defined(TEST_IGNORE_SETADDRESS)
    // Test build: pretend that the first TEST_IGNORE_SETADDRESS SetAddress messages for us got lost, so the bus controller
    // has to send them again. With a high number we never take over an address, and never confirm.
    driver.registerHandler(protocols::Command::SetAddress, "SetAddress handler (test)",
        [&deviceHandler]([[maybe_unused]] protocols::Command command, [[maybe_unused]] uint8_t sender, const std::vector<uint8_t>& data) {
            static unsigned ignored{ 0 };
            if (data.size() != protocols::sizeMsgSetAddress) {
                return;
            }
            protocols::MsgSetAddress msg;
            std::memcpy(&msg.boardId.bytes[0], data.data(), protocols::idSize);
            msg.address = data[protocols::idSize];
            if (msg.boardId.id != deviceHandler.deviceId().id) {
                return;
            }
            if (ignored < TEST_IGNORE_SETADDRESS) {
                ++ignored;
                printf("Test: ignoring SetAddress for 0x%02x (%u of %u)\n", msg.address, ignored, static_cast<unsigned>(TEST_IGNORE_SETADDRESS));
                return;
            }
            deviceHandler.handle(msg);
        });
#endif

    // No address yet: we only hear General Calls until the bus controller assigns one.
    driver.listenAddress(protocols::GeneralCallAddress);
    driver.startListening();

    components::LocalLed internalLed(berry, PICO_DEFAULT_LED_PIN);

#if defined(HAVE_MAX7219)
    // A MAX7219 8-digit display on SPI0 (CS = GP17, SCK = GP18, MOSI = GP19), controlled by messages from the bus controller.
    // Everything lights up for a few seconds, as a check of the wiring.
    interfaces::PicoSPI spi;
    spi.baudRate(500000);
    devices::LocalMAX7219<interfaces::PicoSPI> max(spi);
    max.numDevices(1);
    max.reset();
    printf("Display test on (5 s)\n");
    max.displayTest(1);
    berry.sleepMs(5000);
    max.displayTest(0);
    printf("Display test off\n");

    protocols::MAX7219Handler<interfaces::PicoSPI> maxHandler(max);
    maxHandler.registerAt(driver);
    printf("MAX7219 ready\n");
#endif

#if defined(TEST_TRIGGER)
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_IN);
    gpio_pull_down(TRIGGER_PIN);
    bool triggered{ false };
    printf("Test build: the address request waits for a pulse on GP%u\n", TRIGGER_PIN);
#endif

    printf("Starting main loop\n");
    unsigned ticks = 0;
    uint8_t lastAddress = driver.listenAddress();
    while (true) {
        if (++ticks % 50 == 0) {
            internalLed.toggle();
        }

        driver.processIncoming();

#if defined(TEST_TRIGGER) && !defined(TEST_NO_RETRY)
        if (triggered && (ticks % 100 == 0)) {
#elif defined(TEST_TRIGGER)
        if (false) {    // test build: ask only once, on the trigger
#else
        if (ticks % 100 == 0) {
#endif
            deviceHandler.requestAddressIfNeeded();   // once per second, until we have an address
        }
        if (driver.listenAddress() != lastAddress) {
            lastAddress = driver.listenAddress();
            printf("Now listening on address 0x%02x\n", lastAddress);
#if defined(HAVE_MAX7219)
            max.setNumber(0, lastAddress);      // show our address, until the bus controller shows something else
#endif
        }

#if defined(TEST_TRIGGER)
        // Instead of sleeping for 10 ms, watch the trigger line, so we react within microseconds.
        for (auto end = make_timeout_time_ms(10); absolute_time_diff_us(get_absolute_time(), end) > 0; ) {
            if (!triggered && gpio_get(TRIGGER_PIN) && deviceHandler.haveController()
                && (driver.listenAddress() == protocols::GeneralCallAddress)) {
                triggered = true;
                deviceHandler.requestAddressIfNeeded();
                printf("Triggered: address requested\n");
                break;
            }
        }
#else
        berry.sleepMs(10);
#endif
    }
}
