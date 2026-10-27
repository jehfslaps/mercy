#!/usr/bin/make -f
NAME = Mercy
FILES_DSP = MercyPlugin.cpp MercyBake.cpp
FILES_UI  = MercyUI.cpp MercyDnD.cpp
ifeq ($(shell uname -s),Darwin)
FILES_UI += MercyMac.mm
endif
DPF_TARGET_DIR ?= ../bin/linux
DPF_BUILD_DIR ?= ../build/linux
# MERCY's fixes on top of DPF (patches/*.patch), applied once to the dpf submodule before building
DPF_PATCHED := $(shell cd ../dpf && for p in ../patches/*.patch; do git apply --reverse --check "$$p" 2>/dev/null || git apply "$$p"; done)
include ../dpf/Makefile.plugins.mk
# strict floating point: the same arithmetic on every compiler and CPU, so Windows and Mac sound
# bit-identical (DPF turns on -ffast-math by default; this turns it back off). See MercyMath.hpp.
BUILD_CXX_FLAGS += -I. -O3 -fno-fast-math -ffp-contract=off
ifeq ($(WINDOWS),true)
LINK_FLAGS += -lole32 -luuid -lshell32
TARGETS = vst3
else ifeq ($(MACOS),true)
# Apple Silicon only
BUILD_CXX_FLAGS += -arch arm64 -mmacosx-version-min=11.0
LINK_FLAGS += -arch arm64 -mmacosx-version-min=11.0
TARGETS = vst3 au
else
# Linux: only used for the tests and screenshots (clap is kept here for the state tests)
TARGETS = vst3 clap
endif
all: $(TARGETS)

# cross-platform sound check (test/render.cpp), built with exactly the plugin's DSP flags
render: $(TARGET_DIR)/mercy-render$(APP_EXT)
$(TARGET_DIR)/mercy-render$(APP_EXT): ../test/render.cpp MercyBake.cpp MercyDSP.hpp MercyPresets.hpp MercyBake.hpp
	-@mkdir -p $(TARGET_DIR)
	$(CXX) ../test/render.cpp MercyBake.cpp $(BUILD_CXX_FLAGS) $(LINK_FLAGS) -lpthread -o $@
