# PicoTestI2C

A Raspberry Pi Pico that behaves as a device on an I2C bus run by a bus controller (see `Zero2WTestI2C`). It uses
[CppRaspberry](https://github.com/bert-laverman/CppRaspberry).

It listens on `i2c1` (GP14 = SDA, GP15 = SCL), first without an address, so it only hears the General Calls. When the
bus controller says `Hello`, it answers with its own board id (the flash id of the Pico). The controller then gives it
an address with `SetAddress`, the Pico takes it over and sends a `Hello` from that address to confirm. If the answer does
not come, the Pico asks again once per second. Progress is printed on the USB serial port.

## Build

```bash
export PICO_SDK_PATH=~/pico-sdk
cmake -S . -B build -G Ninja && cmake --build build
```

or use `cppr-deploy PicoTestI2C` from the CppRaspberry tools, which also flashes it.

## Test builds

These are meant for the bus tests in `CppRaspberry/tools/i2c-bus-tests`. Use a separate build directory for each, for
example `build-trigger` (those directories are ignored by git).

| CMake option | Effect |
|---|---|
| `-DTEST_TRIGGER=ON` | The first address request is held back until GP2 goes high, and then sent at once. Connect GP2 of several Picos to one GPIO of the controller to make them ask at the same moment. |
| `-DTEST_IGNORE_SETADDRESS=N` | Pretend the first N `SetAddress` messages got lost. A high N means the Pico never takes over an address and never confirms. |
| `-DTEST_NO_RETRY=ON` | Ask for an address only once (only with `TEST_TRIGGER`). Together with a high `TEST_IGNORE_SETADDRESS`, this lets the controller run out of attempts. |
