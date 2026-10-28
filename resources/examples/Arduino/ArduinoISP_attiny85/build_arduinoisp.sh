#!/bin/sh
# Build ArduinoISP.ino for an Arduino Uno (atmega328p @16MHz) with the Debian arduino-core-avr package
set -e
A=/usr/share/arduino/hardware/arduino/avr
SRC=$1
OUT=$2
W=$(mktemp -d)
FL="-mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR -Os -ffunction-sections -fdata-sections -I$A/cores/arduino -I$A/variants/standard -I$A/libraries/SPI/src"
CXXF="-fno-exceptions -fno-threadsafe-statics -std=gnu++11"
grep -E "^[a-zA-Z_][a-zA-Z0-9_ \*]* [a-zA-Z_][a-zA-Z0-9_]*\([^;]*\) *\{" "$SRC" | sed 's/ *{.*$/;/' > "$W/protos.h"
awk -v P="$W/protos.h" '{print} /^#include "Arduino.h"/ && !done {while((getline l < P)>0) print l; done=1}' "$SRC" > "$W/sketch.cpp"
for f in $A/cores/arduino/*.c; do avr-gcc $FL -c "$f" -o "$W/$(basename $f).o"; done
for f in $A/cores/arduino/*.cpp $A/libraries/SPI/src/SPI.cpp; do avr-g++ $FL $CXXF -c "$f" -o "$W/$(basename $f).o"; done
avr-gcc $FL -x assembler-with-cpp -c $A/cores/arduino/wiring_pulse.S -o "$W/wiring_pulse.S.o"
avr-g++ $FL $CXXF -c "$W/sketch.cpp" -o "$W/sketch.o"
avr-gcc -mmcu=atmega328p -Os -Wl,--gc-sections -o "$W/out.elf" "$W/sketch.o" $W/*.c.o $W/*.cpp.o "$W/wiring_pulse.S.o" -lm
avr-objcopy -O ihex -R .eeprom "$W/out.elf" "$OUT"
avr-size "$W/out.elf"
