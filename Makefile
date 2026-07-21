# OpenOrbis PS4 Toolchain - Yandex Music Player
# Requires: OpenOrbis SDK installed ($OO_PS4_TOOLCHAIN or $ORBIS_SDK)

# SDK path (Docker uses OO_PS4_TOOLCHAIN, local uses ORBIS_SDK)
SDK_PATH := $(or $(OO_PS4_TOOLCHAIN),$(ORBIS_SDK),/lib/OpenOrbisSDK)

# Toolchain
CC := $(SDK_PATH)/bin/linux/orbis-clang
STRIP := $(SDK_PATH)/bin/linux/orbis-strip

# Project
TITLE := Yandex Music Player
TITLEID := DEAD0001
APP_VER := 01.00

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
STB_DIR := $(SDK_PATH)/include/stb

# Sources
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# JSON library (bundled)
JSON_SRC := $(SRC_DIR)/cjson/cJSON.c
JSON_OBJ := $(BUILD_DIR)/cjson.o

# Flags
CFLAGS := -Wall -Wextra -O2 -g
CFLAGS += -I$(INC_DIR) -I$(SDK_PATH)/include -I$(STB_DIR)
CFLAGS += -D__ORBIS__ -DPS4

# Uncomment to enable projectM (Milkdrop) visualizer:
# CFLAGS += -DUSE_PROJECTM

# Libraries
LDFLAGS := -L$(SDK_PATH)/lib
LDFLAGS += -lSceLibc -lSceNet -lSceSysmodule -lSceAudioOut -lSceHttp
LDFLAGS += -lSceIpmi -lSceRtc -lSceSystemService -lSceAppMgr
LDFLAGS += -lScePfs -lSceAppDb

# OpenGL ES (piglet) для визуализатора
LDFLAGS += -lScePiglet -lSceDisplay -lGLESv2 -lEGL

# projectM (если скомпилирована отдельно)
# LDFLAGS += -lprojectM-4

# OGG Vorbis (из oosdk_libraries, если скомпилированы)
# LDFLAGS += -lvorbis -logg

.PHONY: all clean create_sfo package

all: $(BUILD_DIR)/$(TITLEID).elf package

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
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	$(STRIP) $@

# Package as PKG
package: $(BUILD_DIR)/$(TITLEID).elf create_sfo
	@mkdir -p $(BUILD_DIR)/sce_sys
	@mkdir -p $(BUILD_DIR)/sce_sys/package
	python3 tools/create_pkg.py $(BUILD_DIR) $(BUILD_DIR)/$(TITLEID).pkg $(TITLEID)

clean:
	rm -rf $(BUILD_DIR)
