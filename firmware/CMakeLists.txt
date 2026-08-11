cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vanguard_firmware)

# RadioLib's own CMakeLists.txt (managed_components/jgromes__radiolib/CMakeLists.txt)
# doesn't define ARDUINO when building as an ESP-IDF component, so its
# Arduino-specific implementation (ArduinoHal, the SPIClass-based Module
# constructor, etc.) never gets compiled in -- everything guarded by
# `#if ARDUINO >= 100` (see BuildOpt.h) is silently dropped, producing a
# link-time "undefined reference" for any Arduino-flavored RadioLib API used
# from application code. Fix: define ARDUINO for the radiolib component
# target directly, matching the value arduino-esp32's own CMakeLists.txt
# uses for its own sources (-DARDUINO=10812), so both components agree on
# being "Arduino platform" and RadioLib's ArduinoHal path gets compiled.
idf_component_get_property(radiolib_lib jgromes__radiolib COMPONENT_LIB)
target_compile_definitions(${radiolib_lib} PUBLIC ARDUINO=10812)

# RadioLib's own CMakeLists.txt doesn't REQUIRES espressif__arduino-esp32
# either, so with ARDUINO now defined, BuildOpt.h's `#include "Arduino.h"`
# can't find the header. Link against arduino-esp32's component lib so its
# INTERFACE include directories propagate.
idf_component_get_property(arduino_lib espressif__arduino-esp32 COMPONENT_LIB)
target_link_libraries(${radiolib_lib} PUBLIC ${arduino_lib})

# ARDUINO_USB_MODE / ARDUINO_USB_CDC_ON_BOOT are legacy Arduino-IDE-style
# preprocessor macros that arduino-esp32's own HardwareSerial/USB source
# checks directly (#ifdef), not IDF Kconfig options -- PlatformIO set these
# via build_flags; here they must be compile definitions on the
# arduino-esp32 component target itself. Without this, Serial.print() calls
# silently go nowhere (ESP-IDF's own console/boot logs still work since
# those use a separate path -- CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG in
# sdkconfig.defaults -- which is why boot logs appeared but nothing our own
# code printed did).
target_compile_definitions(${arduino_lib} PUBLIC ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1)
