#!/usr/bin/env python3
"""
Accuracy verification: compare pure-C inference output against ONNX Runtime.

Must be run from the cnn_inference/ directory after `make native`.
"""
import numpy as np
import onnxruntime as ort
import subprocess, struct, sys, os

MODEL_PATH = "/home/legion/Downloads/battery_cnn_int8.onnx"

def main():
    print("=" * 60)
    print("CNN Inference Accuracy Verification")
    print("  Pure C engine  vs  ONNX Runtime reference")
    print("=" * 60)

    # ── Generate test inputs ──
    np.random.seed(42)
    inputs = []
    for i in range(5):
        # Random input in [0, 1] range (typical for normalized IC curves)
        x = np.random.rand(2, 128).astype(np.float32)
        inputs.append(x)

    # ── ONNX Runtime reference ──
    print("\n[1/3] ONNX Runtime reference inference...")
    session = ort.InferenceSession(MODEL_PATH)
    ref_results = []
    for x in inputs:
        out = session.run(None, {"ic_curve": x[np.newaxis, :, :]})
        # out[0] = stage_logits, out[1] = rul
        ref_results.append({
            "stage_logits": out[0][0].copy(),
            "rul": out[1][0][0]
        })
    print(f"  OK. {len(ref_results)} samples inferred.")

    # ── C engine inference ──
    print("\n[2/3] Pure C engine inference...")
    c_binary = os.path.join(os.path.dirname(__file__), "..", "build", "test_cnn")
    if not os.path.exists(c_binary):
        print(f"  ERROR: C binary not found at {c_binary}")
        print("  Run 'make native' first.")
        sys.exit(1)

    c_results = []
    for i, x in enumerate(inputs):
        # Write input as binary file
        bin_path = f"/tmp/cnn_test_input_{i}.bin"
        x.astype(np.float32).tofile(open(bin_path, "wb"))

        # Run C binary
        try:
            result = subprocess.run(
                [c_binary, bin_path],
                capture_output=True, text=True, timeout=10
            )
            if result.returncode != 0:
                print(f"  ERROR running C binary (sample {i}):")
                print(result.stderr)
                sys.exit(1)

            # Parse output
            stage_logits = [0.0, 0.0, 0.0]
            rul = 0.0
            for line in result.stdout.split("\n"):
                if "Stage" in line and "logit=" in line:
                    parts = line.split("logit=")
                    if len(parts) == 2:
                        val = float(parts[1].split()[0])
                        if "Stage 0" in line:
                            stage_logits[0] = val
                        elif "Stage 1" in line:
                            stage_logits[1] = val
                        elif "Stage 2" in line:
                            stage_logits[2] = val
                elif "RUL (raw):" in line:
                    rul = float(line.split(":")[-1].strip())

            c_results.append({"stage_logits": stage_logits, "rul": rul})
            print(f"  Sample {i}: OK")
        finally:
            os.unlink(bin_path)

    # ── Compare ──
    print("\n[3/3] Accuracy comparison:")
    print(f"{'Sample':<8} {'Δ stage_logits':<20} {'Δ rul':<15} {'Match?':<8}")
    print("-" * 56)

    all_ok = True
    max_stage_diff = 0.0
    max_rul_diff = 0.0

    for i in range(len(inputs)):
        c = c_results[i]
        r = ref_results[i]

        stage_diff = max(abs(c["stage_logits"][j] - r["stage_logits"][j]) for j in range(3))
        rul_diff = abs(c["rul"] - r["rul"])

        max_stage_diff = max(max_stage_diff, stage_diff)
        max_rul_diff = max(max_rul_diff, rul_diff)

        # Tolerance: 1e-4 for float32 (INT8 quantized model has ~1e-3 inherent noise)
        ok = stage_diff < 1e-3 and rul_diff < 1e-3
        if not ok:
            all_ok = False

        status = "✅" if ok else "❌"
        print(f"  {i:<8} {stage_diff:<20.8f} {rul_diff:<15.8f} {status:<8}")

    print("-" * 56)
    print(f"  Max abs diff:  stage_logits={max_stage_diff:.8f}  rul={max_rul_diff:.8f}")

    if all_ok:
        print("\n  ✅ ALL SAMPLES MATCH within 1e-3 tolerance")
        print("  C inference engine is ACCURATE.")
    else:
        print(f"\n  ⚠️  Some samples exceed 1e-3 tolerance")
        print("  This may indicate a bug in the C implementation or numerical precision diffs.")
        sys.exit(1)

if __name__ == "__main__":
    main()
