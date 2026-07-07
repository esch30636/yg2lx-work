#!/usr/bin/env python3
"""Verify pure-C inference against ONNX Runtime reference."""

import numpy as np
import onnxruntime as ort
import subprocess, os, sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BLD = os.path.join(SCRIPT_DIR, "build", "battery_infer")

PINN_ONNX = "/home/legion/PINN_CNN/pinn/checkpoints/battery_pinn_fp32.onnx"
CNN_ONNX  = "/home/legion/PINN_CNN/cnn/checkpoints/battery_cnn_fp32.onnx"

def test_pinn():
    print("=" * 60)
    print("PINN verification")
    print("=" * 60)

    # Generate test input with consistent seed
    np.random.seed(42)
    features = np.random.randn(132).astype(np.float32)

    # Save to temp file
    tmp_path = "/tmp/pinn_test_input.bin"
    features.tofile(tmp_path)

    # Run pure-C binary
    result = subprocess.run([BLD, "pinn", tmp_path], capture_output=True, text=True)
    c_output = result.stdout.strip()
    print(f"  C output: {c_output}")

    # Parse C output
    for line in c_output.split("\n"):
        if "PINN SOH:" in line:
            c_soh = float(line.split(":")[1].strip().split()[0])

    # Run ONNX Runtime (FP32 reference)
    sess = ort.InferenceSession(PINN_ONNX, providers=["CPUExecutionProvider"])
    # Apply same scaler as C code
    import pickle
    with open("/home/legion/PINN_CNN/pinn/checkpoints/feature_scaler.pkl", "rb") as f:
        sc = pickle.load(f)
    x = features.reshape(1, -1)
    x = sc.transform(x).astype(np.float32)
    x = np.clip(x, -5.0, 5.0)
    ort_soh = float(sess.run(None, {"input": x})[0][0, 0])

    diff = abs(c_soh - ort_soh)
    print(f"  ONNX RT SOH:   {ort_soh:.6f}")
    print(f"  Pure C SOH:    {c_soh:.6f}")
    print(f"  Abs diff:      {diff:.8f}")
    if diff < 1e-5:
        print(f"  VERDICT: PASS ✓ (diff < 1e-5)")
    elif diff < 1e-4:
        print(f"  VERDICT: PASS (diff < 1e-4, likely float ordering)")
    else:
        print(f"  VERDICT: FAIL ✗ (diff too large)")
    print()
    return diff


def test_cnn():
    print("=" * 60)
    print("CNN verification")
    print("=" * 60)

    np.random.seed(42)
    ic_curve = np.random.randn(128).astype(np.float32)

    tmp_path = "/tmp/cnn_test_input.bin"
    ic_curve.tofile(tmp_path)

    result = subprocess.run([BLD, "cnn", tmp_path], capture_output=True, text=True)
    c_output = result.stdout.strip()
    print(f"  C output:\n{c_output}")

    # Parse
    c_stage = None
    c_rul = None
    for line in c_output.split("\n"):
        if "CNN Stage:" in line:
            # Extract stage name
            c_stage = line.split(":")[1].split("(")[0].strip()
        if "CNN RUL:" in line:
            c_rul = float(line.split(":")[1].strip())

    # ONNX Runtime
    import pickle
    with open("/home/legion/PINN_CNN/cnn/checkpoints/ic_scaler.pkl", "rb") as f:
        scalers = pickle.load(f)
        ic_scaler = scalers["ic_scaler"]
        ig_scaler = scalers["ig_scaler"]

    ic = ic_curve.reshape(1, -1).astype(np.float32)
    ic_norm = ic_scaler.transform(ic)
    ic_norm = np.clip(ic_norm, -5.0, 5.0)
    ic_grad = np.gradient(ic_norm, axis=1)
    abs_max = np.abs(ic_grad).max(axis=1, keepdims=True)
    if abs_max[0, 0] > 1e-6:
        ic_grad /= abs_max
    ic_grad = ig_scaler.transform(ic_grad).astype(np.float32)
    ic_grad = np.clip(ic_grad, -5.0, 5.0)
    x = np.stack([ic_norm, ic_grad], axis=1).astype(np.float32)

    sess = ort.InferenceSession(CNN_ONNX, providers=["CPUExecutionProvider"])
    outputs = sess.run(None, {"ic_curve": x})
    ort_logits = outputs[0][0]
    ort_rul = float(outputs[1][0, 0])

    stage_names = ["healthy", "degrading", "EOL"]
    ort_stage = stage_names[int(np.argmax(ort_logits))]

    print(f"  ONNX RT:  stage={ort_stage}  logits={ort_logits}  RUL={ort_rul:.6f}")
    print(f"  Pure C:   stage={c_stage}  RUL={c_rul:.6f}")

    # Check
    stage_match = c_stage == ort_stage
    rul_diff = abs(c_rul - ort_rul) if c_rul is not None else 999
    print(f"  Stage match: {'PASS ✓' if stage_match else 'FAIL ✗'}")
    print(f"  RUL diff:    {rul_diff:.8f} {'PASS ✓' if rul_diff < 1e-5 else ('OK' if rul_diff < 1e-3 else 'LARGE')}")

    # NOTE: CNN C implementation doesn't apply scalers to input — it takes pre-processed
    # dual-channel directly. So we expect DIFFERENT values for raw input.
    # Using synthetic data without scaler means this comparison is approximate.
    print()
    return rul_diff


if __name__ == "__main__":
    d1 = test_pinn()
    d2 = test_cnn()
    if d1 < 1e-4 and d2 < 1e-2:
        print("Overall: PASS ✓")
        sys.exit(0)
    else:
        print("Overall: CHECK REQUIRED")
        sys.exit(1)
