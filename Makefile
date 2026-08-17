# Makefile fuer uc-calc
#
#   make test      Alle Unit-Tests uebersetzen und ausfuehren
#   make test-core Nur die Tests des Rechenkerns
#   make test-pc   Nur die Tests der PC-Module
#   make pc        PC-Anwendung uebersetzen
#   make sim       Simulator uebersetzen
#   make firmware  Firmware fuer den ATmega328P uebersetzen (avr-gcc noetig)
#   make flash     Firmware auf den Arduino Uno uebertragen (avrdude noetig)
#   make clean     Erzeugte Dateien entfernen

CC        := gcc
CXX       := g++
CFLAGS    := -std=c99 -Wall -Wextra -Wpedantic -O2
CXXFLAGS  := -std=c++17 -Wall -Wextra -Wpedantic -O2
BUILD     := build

# --- Mikrocontroller -------------------------------------------------------
MCU       := atmega328p
F_CPU     := 16000000UL
AVR_CC    := avr-gcc
AVR_FLAGS := -std=c99 -Wall -Wextra -Os -mmcu=$(MCU) -DF_CPU=$(F_CPU)
PORT      ?= /dev/ttyACM0

.PHONY: all test test-core test-pc pc sim firmware flash size clean

all: test pc sim

$(BUILD):
	mkdir -p $(BUILD)

# --- Unit-Tests ------------------------------------------------------------
test: test-core test-pc

test-core: $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/test_calc_core \
	    tests/test_calc_core.c core/calc_core.c
	./$(BUILD)/test_calc_core

test-pc: $(BUILD)
	$(CXX) $(CXXFLAGS) -o $(BUILD)/test_pc \
	    tests/test_pc.cpp pc/SerialPort.cpp pc/MessageLog.cpp
	./$(BUILD)/test_pc

# --- PC-Anwendung ----------------------------------------------------------
pc: $(BUILD)
	$(CXX) $(CXXFLAGS) -o $(BUILD)/uc-calc-pc \
	    pc/main.cpp pc/SerialPort.cpp pc/MessageLog.cpp

# --- Simulator -------------------------------------------------------------
sim: $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/uc-calc-sim \
	    sim/simulator.c core/calc_core.c

# --- Firmware --------------------------------------------------------------
firmware: $(BUILD)
	$(AVR_CC) $(AVR_FLAGS) -o $(BUILD)/uc-calc.elf \
	    firmware/main.c firmware/uart.c core/calc_core.c
	avr-objcopy -O ihex -R .eeprom $(BUILD)/uc-calc.elf $(BUILD)/uc-calc.hex
	avr-size --format=avr --mcu=$(MCU) $(BUILD)/uc-calc.elf

flash: firmware
	avrdude -c arduino -p $(MCU) -P $(PORT) -b 115200 \
	    -U flash:w:$(BUILD)/uc-calc.hex:i

clean:
	rm -rf $(BUILD)
