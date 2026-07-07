#=============================================================================
# CNN Inference Engine — Makefile
# Battery SOH model for MYD-YG2LX (Renesas RZ/G2L)
#
# Targets:
#   make              Build native (host) test binary
#   make cross        Cross-compile for aarch64 (MYD-YG2LX target)
#   make verify       Run ONNX Runtime comparison test
#   make clean
#=============================================================================

SRC_DIR  = src
INC_DIR  = include
TST_DIR  = test
BLD_DIR  = build

# Source files
SRCS     = $(SRC_DIR)/cnn_inference.c
HDRS     = $(INC_DIR)/cnn_inference.h $(INC_DIR)/model_weights.h $(INC_DIR)/model_meta.h
TST_SRCS = $(TST_DIR)/test_cnn.c

# ── Native (host) build ──────────────────────────────────────────────
HOST_CC      ?= gcc
HOST_CFLAGS  ?= -O2 -Wall -Wextra -I$(INC_DIR)
HOST_LDFLAGS ?= -lm

native: $(BLD_DIR)/test_cnn

$(BLD_DIR):
	mkdir -p $(BLD_DIR)

$(BLD_DIR)/test_cnn: $(SRCS) $(TST_SRCS) $(HDRS) | $(BLD_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(TST_SRCS) $(SRCS) $(HOST_LDFLAGS)
	@echo "  [OK] Native binary: $@"

# ── Cross-compile for aarch64 (MYD-YG2LX) ────────────────────────────
CROSS_CC      ?= aarch64-poky-linux-gcc
CROSS_CFLAGS  ?= -O2 -mcpu=cortex-a55 -Wall -I$(INC_DIR) $(SDK_CFLAGS)
CROSS_LDFLAGS ?= -lm -static $(SDK_LDFLAGS)

cross: $(BLD_DIR)/test_cnn_aarch64

$(BLD_DIR)/test_cnn_aarch64: $(SRCS) $(TST_SRCS) $(HDRS) | $(BLD_DIR)
	$(CROSS_CC) $(CROSS_CFLAGS) -o $@ $(TST_SRCS) $(SRCS) $(CROSS_LDFLAGS)
	@echo "  [OK] Aarch64 binary: $@"
	@file $@

# ── Production library (for integration into larger projects) ─────────
$(BLD_DIR)/libcnn.a: $(SRCS) $(HDRS) | $(BLD_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -c $(SRCS) -o $(BLD_DIR)/cnn_inference.o
	ar rcs $@ $(BLD_DIR)/cnn_inference.o
	@echo "  [OK] Static library: $@"

$(BLD_DIR)/libcnn_aarch64.a: $(SRCS) $(HDRS) | $(BLD_DIR)
	$(CROSS_CC) $(CROSS_CFLAGS) -c $(SRCS) -o $(BLD_DIR)/cnn_inference_aarch64.o
	aarch64-poky-linux-ar rcs $@ $(BLD_DIR)/cnn_inference_aarch64.o
	@echo "  [OK] Aarch64 static library: $@"

lib: $(BLD_DIR)/libcnn.a
lib-cross: $(BLD_DIR)/libcnn_aarch64.a

# ── Verify against ONNX Runtime ──────────────────────────────────────
verify: $(BLD_DIR)/test_cnn
	@echo "Running accuracy verification..."
	cd $(TST_DIR) && python3 verify_accuracy.py

# ── Clean ────────────────────────────────────────────────────────────
clean:
	rm -rf $(BLD_DIR)
	@echo "Cleaned."

# ── Help ─────────────────────────────────────────────────────────────
help:
	@echo "CNN Inference Engine — Build Targets"
	@echo "  make              Native host build (for testing)"
	@echo "  make cross        Cross-compile for MYD-YG2LX (aarch64)"
	@echo "  make lib          Build host static library"
	@echo "  make lib-cross    Build aarch64 static library"
	@echo "  make verify       Run accuracy check vs ONNX Runtime"
	@echo "  make clean"

.PHONY: native cross lib lib-cross verify clean help
