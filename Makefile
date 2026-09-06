# PokeMini — Pokémon Mini standalone dynamic core for Game & Watch Retro-Go SD.
#
#   make                  — build + pack → PokeMini.bin
#   make host             — Linux/macOS SDL binary → PokeMini_host
#   make host HOST_SDL=3  — same with SDL3
#   make docker           — same build inside Docker (no host toolchain)
#   make docker_shell     — interactive shell in the builder image
#
# Verbose compiler lines: make V=

#######################################
# Project identity
#######################################
PROJECT_KIND ?= core

CORE_NAME  := PokeMini
CORE_ENTRY := app_main

CORE_PKMINI := src/pokemini

CORE_C_SOURCES := \
$(CORE_PKMINI)/freebios/freebios.c \
$(CORE_PKMINI)/source/CommandLine.c \
$(CORE_PKMINI)/source/Hardware.c \
$(CORE_PKMINI)/source/Joystick.c \
$(CORE_PKMINI)/source/MinxAudio.c \
$(CORE_PKMINI)/source/MinxColorPRC.c \
$(CORE_PKMINI)/source/MinxCPU_CE.c \
$(CORE_PKMINI)/source/MinxCPU_CF.c \
$(CORE_PKMINI)/source/MinxCPU_SP.c \
$(CORE_PKMINI)/source/MinxCPU_XX.c \
$(CORE_PKMINI)/source/MinxCPU.c \
$(CORE_PKMINI)/source/MinxIO.c \
$(CORE_PKMINI)/source/MinxIRQ.c \
$(CORE_PKMINI)/source/MinxLCD.c \
$(CORE_PKMINI)/source/MinxPRC.c \
$(CORE_PKMINI)/source/MinxTimers.c \
$(CORE_PKMINI)/source/Multicart.c \
$(CORE_PKMINI)/source/PMCommon.c \
$(CORE_PKMINI)/source/PokeMini.c \
$(CORE_PKMINI)/source/Video_x3.c \
$(CORE_PKMINI)/source/Video.c \
$(CORE_PKMINI)/resource/PokeMini_ColorPal.c \
src/main.c \
src/pkmini_i18n.c

CORE_C_INCLUDES := \
-I$(CORE_PKMINI)/source \
-I$(CORE_PKMINI)/resource \
-I$(CORE_PKMINI)/freebios \
-I$(CORE_PKMINI)/include \
-Isrc

# Relative path so Docker bind-mounts work (do NOT use $(abspath) — it
# bakes the host path into Make prerequisites / .d files). Do not name
# this SDK_ROOT: that env var is commonly set by Android SDK installs.
GNW_CORE_SDK ?= sdk
# Separate build trees so switching PROJECT_KIND does not reuse stale .o.
BUILD_DIR ?= build/$(PROJECT_KIND)

# Hot Minx CPU / video / audio .text in ITCM (see pokemini_core.ld).
CORE_LDSCRIPT := pokemini_core.ld
CORE_EXTRA_SEGMENTS := itcm:core_itcm

#######################################
# Kind-specific compile defs + packing
#######################################
ifeq ($(PROJECT_KIND),core)
# Match release-firmware layout of retro_emulator_file_t: COVERFLOW fields
# sit before cheat_* — CHEAT_CODES alone with COVERFLOW=0 misaligns pointers.
CORE_C_DEFS := \
-DPROJECT_KIND_CORE=1 \
-DCOVERFLOW=1 \
-DCHEAT_CODES=0 \
-DTARGET_GNW

PACKED_BIN  := PokeMini.bin
PAD_LOGO    := src/assets/pad.bmp
HEADER_LOGO := src/assets/header.bmp

else ifeq ($(PROJECT_KIND),homebrew)
$(error This project is a dynamic core only (PROJECT_KIND=core))
else
$(error PROJECT_KIND must be 'core' (got '$(PROJECT_KIND)'))
endif

include $(GNW_CORE_SDK)/Makefile

PACK_CORE := $(GNW_CORE_SDK)/tools/pack_core.py

#######################################
# Packed header version
#######################################
# gnw_core_meta_t only stores major.minor.patch (0..255).
# CORE_VERSION is the full git describe string passed to the packer; it
# extracts the leading vX.Y.Z (NOTAG / missing tags → 0.0.0).
# Override: make CORE_VERSION=v1.2.3
CORE_VERSION ?= $(shell git describe --tags --dirty 2>/dev/null || echo NOTAG)

#######################################
# Pack
#######################################
.PHONY: pack

pack: $(TARGET_BIN) $(PAD_LOGO) $(HEADER_LOGO)
	$(V)$(ECHO) [ PACK CORE ] $(PACKED_BIN) version=$(CORE_VERSION)
	$(V)python3 $(PACK_CORE) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--system name="Pokémon Mini",dirname=mini,pad_logo=$(PAD_LOGO),header_logo=$(HEADER_LOGO),ext=min,parse=rom \
		--core-name "PokeMini" \
		--version "$(CORE_VERSION)" \
		--out $(PACKED_BIN)

all: pack

# Read-only helpers for CI / scripts (make print-PROJECT_KIND, etc.).
.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-DOCKER_IMAGE \
	print-TARGET_ELF print-TARGET_MAP print-CORE_VERSION
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-DOCKER_IMAGE:
	@echo $(DOCKER_IMAGE)
print-TARGET_ELF:
	@echo $(TARGET_ELF)
print-TARGET_MAP:
	@echo $(BUILD_DIR)/$(CORE_NAME)_core.map
print-CORE_VERSION:
	@echo $(CORE_VERSION)

clean::
	$(V)rm -f $(PACKED_BIN)

#######################################
# Docker (same image as firmware repo)
#######################################
.PHONY: docker docker_pull docker_shell

RELEASE_VERSION ?= v1.5
DOCKER_REPOSITORY ?= sylverb/retro-go-sd-builder
DOCKER_IMAGE ?= $(DOCKER_REPOSITORY):$(RELEASE_VERSION)

DOCKER_TTY_FLAG := $(shell if [ -t 0 ]; then echo -it; else echo; fi)
# Host UID so build/ artifacts are not root-owned on the bind mount.
DOCKER_USER := $(shell id -u):$(shell id -g)
DOCKER_RUN := docker run --rm $(DOCKER_TTY_FLAG) \
	--user $(DOCKER_USER) \
	-v "$(CURDIR):/opt/workdir" \
	-w /opt/workdir \
	$(DOCKER_IMAGE)

docker:
	$(V)$(ECHO) "[ DOCKER ]" $(DOCKER_IMAGE) "PROJECT_KIND=$(PROJECT_KIND)"
	$(V)$(DOCKER_RUN) make --no-print-directory -j$$(nproc) PROJECT_KIND=$(PROJECT_KIND)

docker_pull:
	$(V)$(ECHO) "[ PULL ]" $(DOCKER_IMAGE)
	$(V)docker pull $(DOCKER_IMAGE)

docker_shell:
	$(DOCKER_RUN) bash

#######################################
# Host SDL (Linux / macOS)
#######################################
include host/Makefile.host
