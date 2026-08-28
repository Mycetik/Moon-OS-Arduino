MCU = atmega328p
F_CPU = 16000000UL
BAUD = 115200


CC = avr-gcc
CXX = avr-g++
OBJCOPY = avr-objcopy
AVRDUDE = avrdude


CFLAGS = -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -I./kernel -I./pins/standard -I./lib -I./kernel/moon_os -I./kernel/arduino_core -ffunction-sections -fdata-sections
CXXFLAGS = $(CFLAGS) -std=gnu++11

C_SOURCES = $(wildcard kernel/*.c) $(wildcard src/*.c) $(wildcard lib/*/*/*.c) $(wildcard kernel/moon_os/*.c) $(wildcard kernel/arduino_core/*.c)
CXX_SOURCES = $(wildcard kernel/*.cpp) $(wildcard src/*.cpp) $(wildcard lib/*/*/*.cpp) $(wildcard kernel/moon_os/*.cpp) $(wildcard kernel/arduino_core/*.cpp)
S_SOURCES = $(wildcard kernel/*.S) $(wildcard src/*.S) $(wildcard lib/*/*/*.S) $(wildcard kernel/moon_os/*.S) $(wildcard kernel/arduino_core/*.S)

OBJECTS = $(C_SOURCES:.c=.c.o) $(CXX_SOURCES:.cpp=.cpp.o) $(S_SOURCES:.S=.S.o)

TARGET = project



all: $(TARGET).hex

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(TARGET).elf: $(OBJECTS)
	$(CC) -mmcu=$(MCU) -Wl,--gc-sections -Wl,-Map=$(TARGET).map,--cref $(OBJECTS) -o $@ -lm



%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.S.o: %.S
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

upload: $(TARGET).hex
	@if [ -z "$(PORT)" ]; then echo "Не вказано PORT!"; exit 1; fi
	$(AVRDUDE) -F -V -c arduino -p $(MCU) -P $(PORT) -b $(BAUD) -U flash:w:$<

clean:
	rm -f $(OBJECTS) $(TARGET).elf $(TARGET).hex

.PHONY: all upload clean