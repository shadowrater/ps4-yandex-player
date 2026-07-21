# OpenOrbis PS4 Toolchain - Yandex Music Player
# Requires: OpenOrbis SDK ($OO_PS4_TOOLCHAIN or $ORBIS_SDK)

# SDK path
SDK_PATH := $(or $(OO_PS4_TOOLCHAIN),$(ORBIS_SDK),$(HOME)/orbis-sdk)

# Toolchain - use wrapper scripts or system clang
CC := $(SDK_PATH)/bin/linux/orbis-clang
STRIP := $(SDK_PATH)/bin/linux/orbis-strip

# Fallback to system clang if wrappers don't exist
ifeq ($(wildcard $(CC)),)
  CC := clang
  STRIP := llvm-strip
endif

# Project
TITLE := Yandex Music Player
TITLEID := DEAD0001
APP_VER := 01.00

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
SDK_INC := $(SDK_PATH)/include

# Sources
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# JSON library (bundled)
JSON_SRC := $(SRC_DIR)/cjson/cJSON.c
JSON_OBJ := $(BUILD_DIR)/cjson.o

# Flags for PS4 cross-compilation
CFLAGS := -Wall -Wextra -O2 -g
CFLAGS += -I$(INC_DIR) -I$(SDK_INC)
CFLAGS += -target x86_64-pc-freebsd-elf
CFLAGS += -mno-red-zone
CFLAGS += -fms-extensions
CFLAGS += -fno-exceptions
CFLAGS += -fno-rtti
CFLAGS += -ffreestanding
CFLAGS += -D__ORBIS__ -DPS4

# Uncomment to enable projectM (Milkdrop) visualizer:
# CFLAGS += -DUSE_PROJECTM

# Libraries
LDFLAGS := -L$(SDK_PATH)/lib
LDFLAGS += -lSceLibc -lSceNet -lSceSysmodule -lSceAudioOut -lSceHttp
LDFLAGS += -lSceIpmi -lSceRtc -lSceSystemService -lSceAppMgr
LDFLAGS += -lScePfs -lSceAppDb

# OpenGL ES (piglet) для визуализатора (закомментировано пока нет lib)
# LDFLAGS += -lScePiglet -lSceDisplay -lGLESv2 -lEGL

# Linker flags
LDFLAGS += -fuse-ld=lld
LDFLAGS += --eh-frame-hdr
LDFLAGS += --no-dynamic-linker

.PHONY: all clean create_sfo package

all: $(BUILD_DIR)/$(TITLEID).elf

# Create param.sfo
create_sfo:
	python3 tools/create_sfo.py $(TITLEID) "$(TITLE)" $(APP_VER) $(BUILD_DIR)/sce_sys/param.sfo

# Compile CJSON
$(JSON_OBJ): $(JSON_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BUILD_DIR)/$(TITLEID).elf: $(OBJECTS) $(JSON_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	$(STRIP) $@

clean:
	rm -rf $(BUILD_DIR)

# Info
info:
	@echo "SDK: $(SDK_PATH)"
	@echo "CC: $(CC)"
	@echo "Sources: $(SOURCES)"
