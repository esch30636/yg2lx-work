#!/usr/bin/env python3
"""
Debug script: numpy float32 reimplementation of the exact ONNX graph
to verify the C inference engine layer-by-layer.
"""
import numpy as np
import onnx
import onnxruntime as ort
import os, sys

MODEL_PATH = "/home/legion/Downloads/battery_cnn_int8.onnx"
WEIGHTS_DIR = "/home/legion/yg2lx-work/cnn_inference/include"

# Load and dequantize all weights
model = onnx.load(MODEL_PATH)
init = {}
for w in model.graph.initializer:
    arr = onnx.numpy_helper.to_array(w)
    init[w.name] = arr

# Build dequantized weight dict
W = {}
def get_w(name):
    """Dequantize if needed"""
    q_name = name + "_quantized"
    if q_name in init:
        scale = init[name + "_scale"].item()
        zp = init[name + "_zero_point"].item()
        return (init[q_name].astype(np.float32) - zp) * scale
    return init[name]

weight_names = [
    "onnx::Conv_190", "onnx::Conv_191",
    "onnx::Conv_193", "onnx::Conv_194",
    "onnx::Conv_196", "onnx::Conv_197",
    "onnx::Conv_199", "onnx::Conv_200",
    "onnx::Conv_202", "onnx::Conv_203",
    "onnx::Conv_205", "onnx::Conv_206",
    "onnx::Conv_208", "onnx::Conv_209",
    "onnx::Conv_211", "onnx::Conv_212",
    "onnx::Conv_214", "onnx::Conv_215",
    "cls_head.0.bias", "cls_head.0.weight",
    "cls_head.3.bias", "cls_head.3.weight",
    "cls_head.6.bias", "cls_head.6.weight",
    "rul_head.0.bias", "rul_head.0.weight",
    "rul_head.3.bias", "rul_head.3.weight",
    "rul_head.6.bias", "rul_head.6.weight",
]
for n in weight_names:
    try:
        W[n] = get_w(n)
    except KeyError:
        print(f"  WARNING: weight {n} not found")

def gelu(x):
    import math
    erf_vec = np.vectorize(math.erf)
    return x * 0.5 * (1.0 + erf_vec(x / np.sqrt(2.0)))

def conv1d_numpy(x, weight, bias, stride, pad):
    """x: [C_in, L], weight: [C_out, C_in, K]"""
    c_out, c_in, k = weight.shape
    in_len = x.shape[1]
    out_len = (in_len - k + 2*pad) // stride + 1

    # Pad
    if pad > 0:
        x_pad = np.pad(x, ((0,0),(pad,pad)), mode='constant')
    else:
        x_pad = x

    out = np.zeros((c_out, out_len), dtype=np.float32)
    for oc in range(c_out):
        for t in range(out_len):
            start = t * stride
            patch = x_pad[:, start:start+k]
            out[oc, t] = np.sum(patch * weight[oc]) + (bias[oc] if bias is not None else 0)
    return out

def maxpool1d_numpy(x, kernel, stride):
    c, l = x.shape
    out_len = (l - kernel) // stride + 1
    out = np.zeros((c, out_len), dtype=np.float32)
    for oc in range(c):
        for t in range(out_len):
            out[oc, t] = np.max(x[oc, t*stride:t*stride+kernel])
    return out

def linear_numpy(x, weight, bias):
    """ONNX Gemm: weight is [in_features, out_features]"""
    return x @ weight + bias

# ── Test ──
def main():
    np.random.seed(42)
    x = np.random.rand(2, 128).astype(np.float32)

    print("=" * 60)
    print("Layer-by-layer numpy reimplementation")
    print("=" * 60)

    # ONNX Runtime reference
    session = ort.InferenceSession(MODEL_PATH)
    ort_out = session.run(None, {"ic_curve": x[np.newaxis, :, :]})
    ref_logits = ort_out[0][0]
    ref_rul = ort_out[1][0][0]

    print(f"\nONNX Runtime reference:")
    print(f"  stage_logits: {ref_logits}")
    print(f"  rul: {ref_rul:.6f}")

    # ── Numpy reimplementation ──
    # Stem: Conv1D(2→16, k7, s2, p3) + GELU
    y = conv1d_numpy(x, W["onnx::Conv_190"], W["onnx::Conv_191"], stride=2, pad=3)
    y = gelu(y)
    print(f"\nStem: shape={y.shape}  mean={y.mean():.6f}  std={y.std():.6f}")
    stem_out = y.copy()

    # Body Block 0: identity skip
    skip = stem_out.copy()
    y = conv1d_numpy(stem_out, W["onnx::Conv_193"], W["onnx::Conv_194"], stride=1, pad=3)
    y = gelu(y)
    y = conv1d_numpy(y, W["onnx::Conv_196"], W["onnx::Conv_197"], stride=1, pad=3)
    y = gelu(y)
    y = y + skip
    y = maxpool1d_numpy(y, kernel=2, stride=2)
    print(f"Body0+Pool: shape={y.shape}  mean={y.mean():.6f}")

    # Body Block 1: channel-expanding shortcut
    skip_raw = y.copy()
    y = conv1d_numpy(y, W["onnx::Conv_202"], W["onnx::Conv_203"], stride=1, pad=3)
    y = gelu(y)
    y = conv1d_numpy(y, W["onnx::Conv_205"], W["onnx::Conv_206"], stride=1, pad=3)
    y = gelu(y)
    sc = conv1d_numpy(skip_raw, W["onnx::Conv_199"], W["onnx::Conv_200"], stride=1, pad=0)
    y = y + sc
    y = maxpool1d_numpy(y, kernel=2, stride=2)
    print(f"Body1+Pool: shape={y.shape}  mean={y.mean():.6f}")

    # Body Block 2: channel-expanding shortcut
    skip_raw = y.copy()
    y = conv1d_numpy(y, W["onnx::Conv_211"], W["onnx::Conv_212"], stride=1, pad=2)
    y = gelu(y)
    y = conv1d_numpy(y, W["onnx::Conv_214"], W["onnx::Conv_215"], stride=1, pad=2)
    y = gelu(y)
    sc = conv1d_numpy(skip_raw, W["onnx::Conv_208"], W["onnx::Conv_209"], stride=1, pad=0)
    y = y + sc
    y = maxpool1d_numpy(y, kernel=2, stride=2)
    print(f"Body2+Pool: shape={y.shape}  mean={y.mean():.6f}")

    # GAP
    gap = y.mean(axis=1)  # [48]
    print(f"GAP: shape={gap.shape}  mean={gap.mean():.6f}")

    # cls_head
    f = linear_numpy(gap, W["cls_head.0.weight"], W["cls_head.0.bias"])
    f = gelu(f)
    f = linear_numpy(f, W["cls_head.3.weight"], W["cls_head.3.bias"])
    f = gelu(f)
    f = linear_numpy(f, W["cls_head.6.weight"], W["cls_head.6.bias"])
    np_logits = f
    print(f"  cls_head logits: {np_logits}")

    # rul_head
    f = linear_numpy(gap, W["rul_head.0.weight"], W["rul_head.0.bias"])
    f = gelu(f)
    f = linear_numpy(f, W["rul_head.3.weight"], W["rul_head.3.bias"])
    f = gelu(f)
    f = linear_numpy(f, W["rul_head.6.weight"], W["rul_head.6.bias"])
    np_rul = f[0]
    print(f"  rul_head: {np_rul:.6f}")

    # ── Compare ──
    print(f"\n{'='*60}")
    print(f"COMPARISON")
    print(f"{'='*60}")
    print(f"  stage_logits C:  (will compare with C output)")
    print(f"  stage_logits NP: {np_logits}")
    print(f"  stage_logits ORT:{ref_logits}")
    print(f"")
    print(f"  Δ(NP-ORT) stage: {np.max(np.abs(np_logits - ref_logits)):.8f}")
    print(f"  Δ(NP-ORT) rul:   {abs(np_rul - ref_rul):.8f}")

    # Save numpy results for C comparison
    out = {
        "input": x,
        "stem": stem_out,
        "np_logits": np_logits,
        "np_rul": np_rul,
        "ref_logits": ref_logits,
        "ref_rul": ref_rul,
        "gap": gap,
    }
    np.savez("/tmp/cnn_debug.npz", **{k: v for k, v in out.items()})
    print(f"\n  Saved debug data to /tmp/cnn_debug.npz")

if __name__ == "__main__":
    main()
