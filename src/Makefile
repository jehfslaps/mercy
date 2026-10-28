#!/usr/bin/make -f
# "make TEASER=true" builds "000", the teaser version (MERCY with the names scribbled out), as a CLAP
ifeq ($(TEASER),true)
NAME = 000
else
NAME = Mercy
endif
FILES_DSP = MercyPlugin.cpp MercyBake.cpp
FILES_UI  = MercyUI.cpp MercyDnD.cpp
ifeq ($(shell uname -s),Darwin)
FILES_UI += MercyMac.mm
endif
DPF_TARGET_DIR ?= ../bin/linux
DPF_BUILD_DIR ?= ../build/linux
ifeq ($(TEASER),true)
# its own object folder: otherwise it would reuse MERCY's compiled (un-scribbled) files
override DPF_BUILD_DIR := $(DPF_BUILD_DIR)-teaser
endif
# MERCY's fixes on top of DPF (patches/dpf) and its pugl (patches/pugl), applied once before building
DPF_PATCHED := $(shell cd ../dpf && for p in ../patches/dpf/*.patch; do git apply --reverse --check "$$p" 2>/dev/null || git apply "$$p"; done)
PUGL_PATCHED := $(shell cd ../dpf/dgl/src/pugl-upstream && for p in ../../../../patches/pugl/*.patch; do git apply --reverse --check "$$p" 2>/dev/null || git apply "$$p"; done)
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
ifeq ($(TEASER),true)
BUILD_CXX_FLAGS += -DMERCY_TEASER
TARGETS = clap
endif
all: $(TARGETS)

# cross-platform sound check (test/render.cpp), built with exactly the plugin's DSP flags
render: $(TARGET_DIR)/mercy-render$(APP_EXT)
$(TARGET_DIR)/mercy-render$(APP_EXT): ../test/render.cpp MercyBake.cpp MercyDSP.hpp MercyPresets.hpp MercyBake.hpp
	-@mkdir -p $(TARGET_DIR)
	$(CXX) ../test/render.cpp MercyBake.cpp $(BUILD_CXX_FLAGS) $(LINK_FLAGS) -lpthread -o $@
