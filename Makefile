####################
# User config
####################

TARGET_BOARD = MPPT
# TARGET_BOARD = DISCOVERY

#SRC_FOLDER = ./src.old
SRC_FOLDER = ./src
USE_ST_HAL = true

#LINKER_SCRIPT = STM32F030F4P6_exp_FLASH.ld

####################

# Variables specific to the board we use

ifeq ($(TARGET_BOARD),MPPT)
LINKER_SCRIPT ?= STM32F030x4.ld

# Note: from smt32f0xx.h: STM32F030x6 is for both STM32F030x4 and STM32F030x6
MAPPED_DEVICE = STM32F030x6

OPEN_OCD_CONFIG_FILE = ./stm32f0discovery-4kbflash.cfg
else
LINKER_SCRIPT = STM32F051x8.ld

MAPPED_DEVICE = STM32F051x8

OPEN_OCD_CONFIG_FILE = board/stm32f0discovery.cfg
endif

FLASH  = 0x08000000

SERIES_FOLDER = STM32F0xx


#USE_ST_CMSIS = true
#USE_ST_HAL = true

STARTUP_FOLDER = ./startup

SERIES_CPU  = cortex-m0
SERIES_ARCH = armv6-m

RM = rm -rf

# Generic Makefile

ifndef FLASH
    $(error Please set the required FLASH variable in your makefile.)
endif

STM32_CUBE_PATH = ./lib

BIN_FOLDER ?= ./bin
SRC_FOLDER ?= ./src

CC      = arm-none-eabi-gcc
CXX     = arm-none-eabi-g++
LD      = arm-none-eabi-ld -v
AR      = arm-none-eabi-ar
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size

# Default for flags
GCC_FLAGS ?=

# Added flags
GCC_FLAGS += -Os

ifeq ($(TARGET_BOARD),MPPT)
GCC_FLAGS += -DBOARD_MPPT
endif

# Flags - Overall Options
GCC_FLAGS += -specs=nosys.specs

# Flags - C Language Options
GCC_FLAGS += -ffreestanding
# GCC_FLAGS += -std=c11

# Flags - C++ Language Options
#GCC_FLAGS += -fno-threadsafe-statics
#GCC_FLAGS += -fno-rtti
GCC_FLAGS += -fno-exceptions
GCC_FLAGS += -fno-unwind-tables

# Flags - Warning Options
GCC_FLAGS += -Wall
GCC_FLAGS += -Wextra

# Flags - Debugging Options
GCC_FLAGS += -g

# Flags - Optimization Options
GCC_FLAGS += -ffunction-sections
GCC_FLAGS += -fdata-sections

# Flags - Preprocessor options
GCC_FLAGS += -D$(MAPPED_DEVICE)

# Flags - Assembler Options

# Flags - Linker Options
# GCC_FLAGS += -nostdlib
GCC_FLAGS += -Wl,-L.,-T$(LINKER_SCRIPT)

# Flags - Directory Options
GCC_FLAGS += -I./inc
GCC_FLAGS += -I$(STARTUP_FOLDER)

# Flags - Machine-dependant options
GCC_FLAGS += -mcpu=$(SERIES_CPU)
GCC_FLAGS += -march=$(SERIES_ARCH)
GCC_FLAGS += -mlittle-endian
GCC_FLAGS += -mthumb
GCC_FLAGS += -masm-syntax-unified

# Output files
ELF_FILE_NAME ?= stm32f0discovery_executable.elf
BIN_FILE_NAME ?= stm32f0discovery_image.bin
OBJ_STARTUP_FILE_NAME ?= startup_$(MAPPED_DEVICE).o

ELF_FILE_PATH = $(BIN_FOLDER)/$(ELF_FILE_NAME)
BIN_FILE_PATH = $(BIN_FOLDER)/$(BIN_FILE_NAME)
OBJ_STARTUP_FILE_PATH = $(STARTUP_FOLDER)/$(OBJ_STARTUP_FILE_NAME)

# Input files
SRC ?=
SRC += $(wildcard $(SRC_FOLDER)/*.c)

# Include user header files
GCC_FLAGS += -I$(SRC_FOLDER)

# Linker flags
LD_FLAGS = -Xlinker --print-memory-usage

# Startup file
DEVICE_STARTUP = $(STARTUP_FOLDER)/$(MAPPED_DEVICE).s

# Include the CMSIS files, using the HAL implies using the CMSIS
ifneq (,$(or USE_ST_CMSIS, USE_ST_HAL))
    GCC_FLAGS += -D CALL_ARM_SYSTEM_INIT
    GCC_FLAGS += -I$(STM32_CUBE_PATH)/CMSIS/Include
    GCC_FLAGS += -I$(STM32_CUBE_PATH)/CMSIS/$(SERIES_FOLDER)/Include

    SRC += $(wildcard $(STM32_CUBE_PATH)/CMSIS/$(SERIES_FOLDER)/Source/*.c)
endif

# Include the HAL files
ifdef USE_ST_HAL
    GCC_FLAGS += -D USE_HAL_DRIVER
    GCC_FLAGS += -I$(STM32_CUBE_PATH)/STM32F0xx_HAL_Driver/Inc

    SRC += $(wildcard $(STM32_CUBE_PATH)/STM32F0xx_HAL_Driver/Src/*.c)
endif

OBJECT_FILES = $(patsubst %.c,%.o,$(SRC))
HEADERS_SRC = $(shell find src -name *.h)

# Rules

all:$(BIN_FILE_PATH)

$(BIN_FILE_PATH): $(ELF_FILE_PATH)
	$(OBJCOPY) -O binary $^ $@

$(ELF_FILE_PATH): $(OBJECT_FILES) $(OBJ_STARTUP_FILE_PATH) | $(BIN_FOLDER)
	$(CC) $(GCC_FLAGS) $(LD_FLAGS) -o $@ $^

%.o: %.c $(HEADERS_SRC)
	$(CC) $(GCC_FLAGS) -c -o $@ $<

$(OBJ_STARTUP_FILE_PATH): $(DEVICE_STARTUP)
	$(CC) $(GCC_FLAGS) $^ -c -o $@

$(BIN_FOLDER):
	mkdir $(BIN_FOLDER)

clean:
	$(RM) $(BIN_FOLDER)
	$(RM) $(OBJ_STARTUP_FILE_PATH)
	$(RM) $(OBJECT_FILES)

upload: $(BIN_FOLDER)/$(ELF_FILE_NAME)
#	openocd -f interface/stlink-v2.cfg -c "set WORKAREASIZE 0x2000" -f target/stm32f4x_stlink.cfg -c "program build/$(PROJECT).elf verify reset" # Older openocd
	openocd -f $(OPEN_OCD_CONFIG_FILE) -c "reset_config trst_only combined" -c "program $(BIN_FOLDER)/$(ELF_FILE_NAME) verify reset exit" # For openocd 0.9

.PHONY: all clean upload


