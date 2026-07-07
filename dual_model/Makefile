#=============================================================================
# Dual-Model Battery SOH Inference Engine — Makefile
#
# Targets:
#   make              Native host build (for verification)
#   make cross        Cross-compile for aarch64 (MYD-YG2LX)
#   make clean
#=============================================================================

SRC_DIR  = src
INC_DIR  = include
BLD_DIR  = build

# Source files
SRCS     = $(SRC_DIR)/pinn_inference.c $(SRC_DIR)/cnn_inference.c $(SRC_DIR)/battery_infer.c
HDRS     = $(INC_DIR)/battery_inference.h \
            $(INC_DIR)/pinn_weights.h $(INC_DIR)/pinn_meta.h \
            $(INC_DIR)/cnn_weights.h  $(INC_DIR)/cnn_meta.h \
            $(INC_DIR)/scalers.h

# ── Native (host) build ──────────────────────────────────────────
HOST_CC      ?= gcc
HOST_CFLAGS  ?= -O2 -Wall -Wextra -I$(INC_DIR)
HOST_LDFLAGS ?= -lm

native: $(BLD_DIR)/battery_infer

$(BLD_DIR):
	mkdir -p $(BLD_DIR)

$(BLD_DIR)/battery_infer: $(SRCS) $(HDRS) | $(BLD_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(SRCS) $(HOST_LDFLAGS)
	@echo "  [OK] Native binary: $@"

# ── Cross-compile for aarch64 (MYD-YG2LX) ────────────────────────
# Requires: source /opt/yg2lx/environment-setup-aarch64-poky-linux
# SDK sets $CC with --sysroot, use it directly or override
CROSS_CC      ?= $(CC)
CROSS_CFLAGS  ?= -O3 -mcpu=cortex-a55 -Wall -I$(INC_DIR)
CROSS_LDFLAGS ?= -lm -static

cross: $(BLD_DIR)/battery_infer_aarch64

$(BLD_DIR)/battery_infer_aarch64: $(SRCS) $(HDRS) | $(BLD_DIR)
	$(CROSS_CC) $(CROSS_CFLAGS) -o $@ $(SRCS) $(CROSS_LDFLAGS)
	@echo "  [OK] Aarch64 binary: $@"
	@file $@
	@ls -lh $@

# ── Optimized cross (stripped) ────────────────────────────────────
cross-fast: $(BLD_DIR)/battery_infer_aarch64_fast

$(BLD_DIR)/battery_infer_aarch64_fast: $(SRCS) $(HDRS) | $(BLD_DIR)
	$(CROSS_CC) $(CROSS_CFLAGS) -o $@ $(SRCS) $(CROSS_LDFLAGS)
	aarch64-poky-linux-strip $@
	@echo "  [OK] Stripped aarch64 binary: $@"
	@file $@
	@ls -lh $@

# ── Verify on host against ONNX Runtime ───────────────────────────
verify: $(BLD_DIR)/battery_infer
	python3 verify_accuracy.py

# ── Preprocessing verification ─────────────────────────────────────
verify-preprocess: tests/verify_preprocess.cpp hmi/inference/CnnPreprocessor.cpp hmi/inference/CnnPreprocessor.h include/scalers.h
	g++ -O2 -Wall -Iinclude -Ihmi/inference \
		-o tests/verify_preprocess \
		tests/verify_preprocess.cpp hmi/inference/CnnPreprocessor.cpp -lm
	./tests/verify_preprocess 100

# ── Accuracy benchmark ──────────────────────────────────────────
benchmark-accuracy: $(BLD_DIR)/battery_infer
	python3 tests/benchmark_accuracy.py

# ── Clean ─────────────────────────────────────────────────────────
clean:
	rm -rf $(BLD_DIR)
	@echo "Cleaned."

# ── HMI (Qt5) cross-compile ──────────────────────────────────────
HMI_DIR = hmi
HMI_BUILD_DIR = $(HMI_DIR)/build

hmi-cross: $(HMI_BUILD_DIR)/battery_hmi

$(HMI_BUILD_DIR):
	mkdir -p $(HMI_BUILD_DIR)

$(HMI_BUILD_DIR)/battery_hmi: $(shell find hmi -name '*.cpp' -o -name '*.h' -o -name '*.qrc' -o -name 'CMakeLists.txt') $(SRCS) $(HDRS) | $(HMI_BUILD_DIR)
	cd $(HMI_BUILD_DIR) && \
	cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=$(OECORE_NATIVE_SYSROOT)/usr/share/cmake/OEToolchainConfig.cmake \
		-DCMAKE_PREFIX_PATH=$(SDKTARGETSYSROOT)/usr/lib64/cmake \
		-DCMAKE_BUILD_TYPE=MinSizeRel && \
	$(MAKE) -j$$(nproc)
	$(STRIP) $(HMI_BUILD_DIR)/battery_hmi
	@echo "  [OK] HMI binary: $(HMI_BUILD_DIR)/battery_hmi"
	@file $(HMI_BUILD_DIR)/battery_hmi
	@ls -lh $(HMI_BUILD_DIR)/battery_hmi

hmi-clean:
	rm -rf $(HMI_BUILD_DIR)

.PHONY: native cross cross-fast verify clean hmi-cross hmi-clean verify-preprocess benchmark-accuracy
