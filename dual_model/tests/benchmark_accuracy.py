#!/usr/bin/env python3
"""On-board CNN/PINN inference accuracy benchmark against ground truth labels.

Compares pure-C inference engine (build/battery_infer) vs ONNX Runtime reference
vs ground truth labels from NASA PCoE + CALCE test sets.

Usage:
    cd /home/legion/yg2lx-work/dual_model
    make native                    # build C inference binary
    python3 tests/benchmark_accuracy.py

Exit 0 = all metrics within thresholds.
"""
import sys, os, subprocess, pickle, tempfile, time, traceback
import numpy as np
from pathlib import Path
from collections import Counter

# ── Paths ──
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
BLD = PROJECT_DIR / "build" / "battery_infer"
PINN_DIR = Path("/home/legion/PINN_CNN/pinn")
CNN_DIR  = Path("/home/legion/PINN_CNN/cnn")
DATA_DIR = Path("/home/legion/PINN_CNN/data")

PINN_ONNX = PINN_DIR / "checkpoints" / "battery_pinn_fp32.onnx"
CNN_ONNX  = CNN_DIR  / "checkpoints" / "battery_cnn_fp32.onnx"
PINN_SCALER = PINN_DIR / "checkpoints" / "feature_scaler.pkl"
CNN_SCALER  = CNN_DIR  / "checkpoints" / "ic_scaler.pkl"

# ── Add PINN_CNN to path for data loaders ──
PINN_CNN = Path("/home/legion/PINN_CNN")
if str(PINN_CNN) not in sys.path:
    sys.path.insert(0, str(PINN_CNN))

THRESHOLDS = {"healthy": 0.82, "degrading": 0.70}  # 3-stage thresholds

STAGE_NAMES = ["HEALTHY", "DEGRADING", "EOL"]


# ═══════════════════════════════════════════════════════════════
# Data loading
# ═══════════════════════════════════════════════════════════════

def load_all_data():
    """Load NASA + CALCE datasets. Returns merged raw_data dict."""
    from pinn.real_data import load_nasa_pcoe, load_calce
    datasets = []

    nasa = load_nasa_pcoe(DATA_DIR / "nasa_pcoe")
    if len(nasa.get("soh", [])) > 0:
        print(f"  NASA:  {len(nasa['soh'])} samples, {len(set(nasa['cell_id']))} cells")
        datasets.append(nasa)

    # CALCE: try cache first, fall back to loader (needs pandas)
    calce_cache = DATA_DIR / "calce" / "calce_cache.npz"
    calce = None
    if calce_cache.exists():
        cached = dict(np.load(calce_cache, allow_pickle=True))
        calce = {k: np.asarray(v) for k, v in cached.items()}
        print(f"  CALCE (cached): {len(calce['soh'])} samples, {len(set(calce['cell_id']))} cells")
    else:
        try:
            calce = load_calce(DATA_DIR / "calce")
        except ImportError:
            print("  CALCE: skipped (pandas not available, no cache)")
    if calce is not None and len(calce.get("soh", [])) > 0:
        datasets.append(calce)

    # Merge with cell_id offset
    merged = {"cell_id": [], "cycle": [], "soh": [], "temp": [],
              "ic": [], "dv_start": [], "capacity_meas": []}
    offset = 0
    for ds in datasets:
        n = len(ds["soh"])
        for key in merged:
            val = ds[key]
            if key == "cell_id":
                merged[key].append(np.asarray(val, dtype=np.int64) + offset)
            else:
                merged[key].append(np.asarray(val))
        offset += int(np.max(ds["cell_id"])) + 1

    result = {k: np.concatenate(v) for k, v in merged.items()}
    print(f"  Total: {len(result['soh'])} samples, {len(set(result['cell_id']))} cells")
    return result


def cell_split(cell_ids, train_r=0.70, val_r=0.15, seed=42):
    """Cell-based split: returns (train_mask, val_mask, test_mask)."""
    unique = np.unique(cell_ids)
    rng = np.random.default_rng(seed)
    perm = rng.permutation(unique)
    n = len(perm)

    n_train = max(1, int(n * train_r))
    n_val   = max(1, int(n * val_r))
    n_test  = max(1, n - n_train - n_val)

    # Edge case: ensure all sets have at least 1 cell
    if n_train + n_val + n_test > n:
        n_train = max(1, n - n_val - n_test)

    train_cells = set(perm[:n_train])
    val_cells   = set(perm[n_train:n_train + n_val])
    test_cells  = set(perm[n_train + n_val:])

    return (
        np.array([cid in train_cells for cid in cell_ids]),
        np.array([cid in val_cells   for cid in cell_ids]),
        np.array([cid in test_cells  for cid in cell_ids]),
    )


def soh_to_stage(soh):
    """3-stage label: 0=healthy(SOH>=0.82), 1=degrading(0.82>SOH>=0.70), 2=EOL."""
    stages = np.full_like(soh, 2, dtype=np.int64)
    stages[soh >= THRESHOLDS["degrading"]] = 1
    stages[soh >= THRESHOLDS["healthy"]]   = 0
    return stages


def compute_rul(raw_data, eol_threshold=0.70):
    """Normalised RUL ∈ [0, 1] per cell."""
    cell_ids = raw_data["cell_id"]
    soh = raw_data["soh"]
    cycles = raw_data["cycle"]
    rul = np.zeros(len(soh), dtype=np.float32)
    for cid in np.unique(cell_ids):
        mask = cell_ids == cid
        cyc = cycles[mask]
        soh_c = soh[mask]
        order = np.argsort(cyc)
        cyc_s = cyc[order]
        soh_s = soh_c[order]
        below = np.where(soh_s < eol_threshold)[0]
        eol_cycle = cyc_s[below[0]] if len(below) > 0 else cyc_s[-1]
        raw_rul = np.maximum(0, eol_cycle - cyc_s).astype(np.float32)
        norm = raw_rul / max(eol_cycle, 1)
        rul[mask] = norm[order.argsort()]
    return rul


# ═══════════════════════════════════════════════════════════════
# C inference via subprocess
# ═══════════════════════════════════════════════════════════════

def run_c_pinn(features_132):
    """Run C PINN inference. Returns SOH float."""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        f.write(features_132.astype(np.float32).tobytes())
        tmp = f.name
    try:
        r = subprocess.run([str(BLD), "pinn", tmp], capture_output=True, text=True, timeout=5)
        for line in r.stdout.strip().split("\n"):
            if "PINN SOH:" in line:
                return float(line.split(":")[1].strip().split()[0])
        raise RuntimeError(f"C PINN parse error: {r.stdout} {r.stderr}")
    finally:
        os.unlink(tmp)


def run_c_cnn(ic_curve_128):
    """Run C CNN inference. Returns (stage_int, rul_float)."""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        f.write(ic_curve_128.astype(np.float32).tobytes())
        tmp = f.name
    try:
        r = subprocess.run([str(BLD), "cnn", tmp], capture_output=True, text=True, timeout=5)
        stage = None; rul = None
        for line in r.stdout.strip().split("\n"):
            if "CNN Stage:" in line:
                name = line.split(":")[1].split("(")[0].strip()
                stage = {"healthy": 0, "degrading": 1, "eol": 2}.get(name.lower(), -1)
            if "CNN RUL:" in line:
                rul = float(line.split(":")[1].strip())
        if stage is None or rul is None:
            raise RuntimeError(f"C CNN parse error: {r.stdout} {r.stderr}")
        return stage, rul
    finally:
        os.unlink(tmp)


# ═══════════════════════════════════════════════════════════════
# ONNX Runtime inference
# ═══════════════════════════════════════════════════════════════

def build_onnx_runners():
    """Create ONNX Runtime inference sessions."""
    import onnxruntime as ort
    sess_pinn = ort.InferenceSession(str(PINN_ONNX), providers=["CPUExecutionProvider"])
    sess_cnn  = ort.InferenceSession(str(CNN_ONNX),  providers=["CPUExecutionProvider"])

    with open(PINN_SCALER, "rb") as f:
        pinn_scaler = pickle.load(f)
    with open(CNN_SCALER, "rb") as f:
        cnn_scalers = pickle.load(f)

    return sess_pinn, sess_cnn, pinn_scaler, cnn_scalers


def run_onnx_pinn(features_132, sess, scaler):
    """ONNX PINN inference. features_132 = raw 132-d feature vector."""
    x = features_132.reshape(1, -1).astype(np.float32)
    x = scaler.transform(x).astype(np.float32)
    x = np.clip(x, -5.0, 5.0)
    return float(sess.run(None, {"input": x})[0][0, 0])


def run_onnx_cnn(ic_curve_128, sess, scalers, grad_mode="central"):
    """ONNX CNN inference. ic_curve_128 = raw 128-point IC curve.

    grad_mode: 'central' (np.gradient, matches training) or 'forward' (matches C engine)"""
    ic_scaler = scalers["ic_scaler"]
    ig_scaler = scalers["ig_scaler"]

    ic = ic_curve_128.reshape(1, -1).astype(np.float32)
    ic_norm = ic_scaler.transform(ic)
    ic_norm = np.clip(ic_norm, -5.0, 5.0)

    if grad_mode == "forward":
        # Forward difference: matches C engine (battery_infer.c lines 89-96)
        grad = np.zeros(128, dtype=np.float32)
        for i in range(1, 128):
            grad[i] = ic_norm[0, i] - ic_norm[0, i - 1]
    else:
        # Central difference: matches training (np.gradient)
        grad = np.gradient(ic_norm, axis=1)[0]

    abs_max = np.abs(grad).max()
    if abs_max < 1e-6:
        abs_max = 1.0
    grad = grad / abs_max
    grad = ig_scaler.transform(grad.reshape(1, -1)).astype(np.float32)
    grad = np.clip(grad, -5.0, 5.0)
    x = np.stack([ic_norm, grad], axis=1).astype(np.float32)

    outputs = sess.run(None, {"ic_curve": x})
    logits = outputs[0][0]
    stage = int(np.argmax(logits))
    rul = float(outputs[1][0, 0])
    return stage, rul


# ═══════════════════════════════════════════════════════════════
# Metrics
# ═══════════════════════════════════════════════════════════════

def compute_metrics(labels, preds, num_classes=3):
    """Compute accuracy, per-class precision/recall/F1, confusion matrix."""
    labels = np.asarray(labels, dtype=np.int64)
    preds  = np.asarray(preds,  dtype=np.int64)
    n = len(labels)

    acc = (labels == preds).sum() / n

    # Confusion matrix
    cm = np.zeros((num_classes, num_classes), dtype=np.int64)
    for t, p in zip(labels, preds):
        cm[t, p] += 1

    # Per-class
    per_class = {}
    for c in range(num_classes):
        tp = cm[c, c]
        fp = cm[:, c].sum() - tp
        fn = cm[c, :].sum() - tp
        prec = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        rec  = tp / (tp + fn) if (tp + fn) > 0 else 0.0
        f1   = 2 * prec * rec / (prec + rec) if (prec + rec) > 0 else 0.0
        sup  = int(cm[c, :].sum())
        per_class[c] = {"prec": prec, "rec": rec, "f1": f1, "support": sup}

    return acc, cm, per_class


def regression_metrics(y_true, y_pred):
    """MAE, RMSE, R²."""
    yt = np.asarray(y_true, dtype=np.float64)
    yp = np.asarray(y_pred, dtype=np.float64)
    diff = yt - yp
    mae  = float(np.mean(np.abs(diff)))
    rmse = float(np.sqrt(np.mean(diff ** 2)))
    ss_res = np.sum(diff ** 2)
    ss_tot = np.sum((yt - yt.mean()) ** 2)
    r2 = float(1 - ss_res / ss_tot) if ss_tot > 1e-12 else 0.0
    return mae, rmse, r2


# ═══════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════

def main():
    print("═" * 60)
    print("  BATTERY INFERENCE ACCURACY BENCHMARK")
    print("═" * 60)
    print()

    # ── 1. Load data ──
    print("── Loading data ──")
    raw = load_all_data()
    if len(raw["soh"]) == 0:
        print("ERROR: No data loaded"); sys.exit(1)

    # Build features [132]: IC(128) + temp + log10(cycle+1) + dv_start + capacity_meas
    ic_raw = raw["ic"].astype(np.float32)
    if ic_raw.ndim == 1:
        ic_raw = ic_raw.reshape(1, -1)
    ic_raw = ic_raw[:, :128]
    if ic_raw.shape[1] < 128:
        pad = np.zeros((len(ic_raw), 128 - ic_raw.shape[1]), dtype=np.float32)
        ic_raw = np.concatenate([ic_raw, pad], axis=1)
    ic_raw = np.nan_to_num(ic_raw, nan=0.0, posinf=0.0, neginf=0.0)

    temp  = raw["temp"].reshape(-1, 1).astype(np.float32)
    logcy = np.log10(raw["cycle"].reshape(-1, 1) + 1.0).astype(np.float32)
    dv    = raw["dv_start"].reshape(-1, 1).astype(np.float32)
    cap   = raw["capacity_meas"].reshape(-1, 1).astype(np.float32)

    aux = np.concatenate([temp, logcy, dv, cap], axis=1)
    aux = np.nan_to_num(aux, nan=0.0, posinf=1e6, neginf=-1e6)
    features_raw = np.concatenate([ic_raw, aux], axis=1).astype(np.float32)

    # ── 2. Split (cell-based, seed=42) ──
    cell_ids = raw["cell_id"].astype(np.int64)
    soh_arr  = raw["soh"].astype(np.float32)
    cycles   = raw["cycle"].astype(np.int64)

    train_mask, val_mask, test_mask = cell_split(cell_ids, 0.70, 0.15, seed=42)
    print(f"  Split: train={train_mask.sum()} val={val_mask.sum()} test={test_mask.sum()}")
    test_cells = set(cell_ids[test_mask])
    print(f"  Test cells: {sorted(test_cells)}")

    if test_mask.sum() == 0:
        print("ERROR: No test samples"); sys.exit(2)

    # ── 3. Ground truth labels ──
    stages = soh_to_stage(soh_arr)
    rul    = compute_rul(raw)

    # Extract test set
    test_idx = np.where(test_mask)[0]
    N = len(test_idx)
    print(f"\n  Test samples: {N}")

    test_ic       = ic_raw[test_idx]
    test_features = features_raw[test_idx]
    test_soh      = soh_arr[test_idx]
    test_stage    = stages[test_idx]
    test_rul      = rul[test_idx]

    stage_dist = Counter(test_stage)
    print(f"  Stage distribution: H={stage_dist.get(0,0)} D={stage_dist.get(1,0)} EOL={stage_dist.get(2,0)}")
    print(f"  SOH range: [{test_soh.min():.3f}, {test_soh.max():.3f}]")
    print()

    # ── 4. Check C binary ──
    if not BLD.exists():
        print("Building C inference binary...")
        subprocess.run(["make", "-C", str(PROJECT_DIR), "native"], check=True)
    if not BLD.exists():
        print("ERROR: C binary not found"); sys.exit(3)

    # ── 5. Run C inference ──
    print(f"── C Inference ({N} samples) ──")
    t0 = time.time()

    c_pinn_soh = np.zeros(N, dtype=np.float32)
    c_cnn_stage = np.zeros(N, dtype=np.int32)
    c_cnn_rul   = np.zeros(N, dtype=np.float32)
    c_errors = 0

    for i in range(N):
        try:
            c_pinn_soh[i] = run_c_pinn(test_features[i])
            stage, rul = run_c_cnn(test_ic[i])
            c_cnn_stage[i] = stage
            c_cnn_rul[i]   = rul
        except Exception as e:
            c_errors += 1
            c_pinn_soh[i] = 0.0
            c_cnn_stage[i] = -1
            c_cnn_rul[i]   = 0.0
            if c_errors <= 3:
                print(f"  [warn] sample {i}: {e}")

        if (i + 1) % max(1, N // 10) == 0:
            print(f"  {i+1}/{N} ({100*(i+1)//N}%)")

    t_c = time.time() - t0
    print(f"  Done in {t_c:.1f}s ({t_c*1000/N:.1f} ms/sample)")
    if c_errors > 0:
        print(f"  ⚠ {c_errors} C inference errors (results set to 0)")
    print()

    # ── 6. Run ONNX inference ──
    # C engine now uses central difference (matches training). We still run
    # forward diff for historical comparison / regression check.
    print(f"── ONNX Runtime Inference ({N} samples, central+forward) ──")
    t0 = time.time()

    sess_pinn, sess_cnn, pinn_scaler, cnn_scalers = build_onnx_runners()

    onnx_pinn_soh  = np.zeros(N, dtype=np.float32)
    onnx_cnn_stage_c = np.zeros(N, dtype=np.int32)   # central diff (training, now matches C)
    onnx_cnn_rul_c   = np.zeros(N, dtype=np.float32)
    onnx_cnn_stage_f = np.zeros(N, dtype=np.int32)   # forward diff (legacy C, pre-fix)
    onnx_cnn_rul_f   = np.zeros(N, dtype=np.float32)
    onnx_errors = 0

    for i in range(N):
        try:
            onnx_pinn_soh[i]   = run_onnx_pinn(test_features[i], sess_pinn, pinn_scaler)
            sc, rc = run_onnx_cnn(test_ic[i], sess_cnn, cnn_scalers, grad_mode="central")
            sf, rf = run_onnx_cnn(test_ic[i], sess_cnn, cnn_scalers, grad_mode="forward")
            onnx_cnn_stage_c[i] = sc
            onnx_cnn_rul_c[i]   = rc
            onnx_cnn_stage_f[i] = sf
            onnx_cnn_rul_f[i]   = rf
        except Exception as e:
            onnx_errors += 1
            onnx_pinn_soh[i]   = 0.0
            onnx_cnn_stage_c[i] = -1
            onnx_cnn_rul_c[i]   = 0.0
            onnx_cnn_stage_f[i] = -1
            onnx_cnn_rul_f[i]   = 0.0
            if onnx_errors <= 3:
                print(f"  [warn] sample {i}: {e}")

        if (i + 1) % max(1, N // 10) == 0:
            print(f"  {i+1}/{N} ({100*(i+1)//N}%)")

    t_onnx = time.time() - t0
    print(f"  Done in {t_onnx:.1f}s ({t_onnx*1000/N:.1f} ms/sample)")
    if onnx_errors > 0:
        print(f"  ⚠ {onnx_errors} ONNX errors")
    print()

    # ── 7. Compute metrics ──
    valid = c_cnn_stage >= 0
    n_valid = valid.sum()
    if n_valid < N:
        print(f"  Filtering to {n_valid}/{N} valid samples\n")

    # --- CNN Stage ---
    print("═══ CNN STAGE CLASSIFICATION ═══")
    c_stage_acc, c_cm, c_pc = compute_metrics(test_stage[valid], c_cnn_stage[valid])
    o_stage_acc_c, o_cm_c, o_pc_c = compute_metrics(test_stage[valid], onnx_cnn_stage_c[valid])
    o_stage_acc_f, o_cm_f, o_pc_f = compute_metrics(test_stage[valid], onnx_cnn_stage_f[valid])

    # C vs ONNX consistency: BOTH now use central diff (should be ~100%)
    stage_match_central = (c_cnn_stage[valid] == onnx_cnn_stage_c[valid]).sum() / n_valid
    # C vs ONNX consistency: DIFFERENT preprocessing (central vs forward, historical)
    stage_match_mix = (c_cnn_stage[valid] == onnx_cnn_stage_f[valid]).sum() / n_valid

    print(f"  ONNX (central diff = training): accuracy = {o_stage_acc_c*100:.1f}%")
    print(f"  C Engine (central diff = fixed):  accuracy = {c_stage_acc*100:.1f}%")
    print(f"  ONNX (forward diff = legacy C):   accuracy = {o_stage_acc_f*100:.1f}%  [pre-fix baseline]")
    print()
    print(f"  C ↔ ONNX (both central):  match = {stage_match_central*100:.1f}% {'✅ VERIFIED' if stage_match_central > 0.999 else '⚠ CHECK'} ")
    print(f"  C ↔ ONNX (ctr vs fwd):    match = {stage_match_mix*100:.1f}% {'⚠ PRE-FIX MISMATCH' if stage_match_mix < 0.99 else ''}")
    print()

    print("  Per-class (C engine, central diff):")
    print(f"          {'Prec':>6}  {'Recall':>6}  {'F1':>6}  {'Support':>7}")
    for c in range(3):
        pc = c_pc[c]
        print(f"  {STAGE_NAMES[c]:8s}  {pc['prec']:6.3f}  {pc['rec']:6.3f}  {pc['f1']:6.3f}  {pc['support']:7d}")
    print()
    print("  Confusion matrix (C engine):")
    print(f"           pred_H  pred_D  pred_EOL")
    for c in range(3):
        print(f"  true_{STAGE_NAMES[c]:5s}  {c_cm[c,0]:6d}  {c_cm[c,1]:6d}  {c_cm[c,2]:8d}")
    print()

    # --- CNN RUL ---
    print("═══ CNN RUL REGRESSION ═══")
    c_rul_mae, c_rul_rmse, c_rul_r2 = regression_metrics(test_rul[valid], c_cnn_rul[valid])
    o_rul_mae_c, o_rul_rmse_c, o_rul_r2_c = regression_metrics(test_rul[valid], onnx_cnn_rul_c[valid])
    o_rul_mae_f, o_rul_rmse_f, o_rul_r2_f = regression_metrics(test_rul[valid], onnx_cnn_rul_f[valid])

    # RUL diff: C vs ONNX with SAME preprocessing (both central)
    rul_diff_same = np.abs(c_cnn_rul[valid] - onnx_cnn_rul_c[valid])
    rul_diff_mix  = np.abs(c_cnn_rul[valid] - onnx_cnn_rul_f[valid])

    print(f"  ONNX (central diff):  MAE={o_rul_mae_c:.4f}  RMSE={o_rul_rmse_c:.4f}  R²={o_rul_r2_c:.4f}")
    print(f"  C Engine (central):   MAE={c_rul_mae:.4f}  RMSE={c_rul_rmse:.4f}  R²={c_rul_r2:.4f}")
    print(f"  ONNX (forward diff):  MAE={o_rul_mae_f:.4f}  RMSE={o_rul_rmse_f:.4f}  R²={o_rul_r2_f:.4f}  [pre-fix]")
    print()
    print(f"  C ↔ ONNX (both central): RUL diff max={rul_diff_same.max():.6f}  mean={rul_diff_same.mean():.6f}")
    print(f"  C ↔ ONNX (ctr vs fwd):   RUL diff max={rul_diff_mix.max():.6f}  mean={rul_diff_mix.mean():.6f}  [pre-fix gap]")
    print()

    # --- PINN SOH ---
    print("═══ PINN SOH REGRESSION ═══")
    c_soh_mae, c_soh_rmse, c_soh_r2 = regression_metrics(test_soh[valid], c_pinn_soh[valid])
    o_soh_mae, o_soh_rmse, o_soh_r2 = regression_metrics(test_soh[valid], onnx_pinn_soh[valid])
    soh_diff = np.abs(c_pinn_soh[valid] - onnx_pinn_soh[valid])

    print(f"  C Engine:      MAE={c_soh_mae:.4f}  RMSE={c_soh_rmse:.4f}  R²={c_soh_r2:.4f}")
    print(f"  ONNX Runtime:  MAE={o_soh_mae:.4f}  RMSE={o_soh_rmse:.4f}  R²={o_soh_r2:.4f}")
    print(f"  C ↔ ONNX SOH diff: max={soh_diff.max():.6f}  mean={soh_diff.mean():.6f}")
    print()

    # ── 8. Findings summary ──
    print("═" * 60)
    print("  FINDINGS")
    print("═" * 60)

    if stage_match_central > 0.999:
        print("  ✅ C engine output = ONNX (both central diff, matching training)")
        print("     → C inference engine is CORRECT — gradient fix verified")
    else:
        print(f"  ⚠  C vs ONNX (both central): {stage_match_central*100:.1f}% match — check needed")

    if stage_match_central > 0.999 and stage_match_mix < 0.99:
        print(f"  ✅ GRADIENT BUG FIXED: C now uses central diff (was forward diff)")
        print(f"     Stage match ctr→fwd: {stage_match_mix*100:.1f}% — old gap closed")
        print(f"     Files: battery_infer.c lines 89-106, CnnPreprocessor.cpp computeGradient()")

    print(f"  Model quality (C engine / ONNX central diff = training preprocessing):")
    print(f"     Stage accuracy: {o_stage_acc_c*100:.1f}%  |  RUL MAE: {o_rul_mae_c:.4f}")
    if o_stage_acc_c < 0.70:
        print(f"     ⚠  Low accuracy — model biased toward majority class (HEALTHY)")
        print(f"     Possible causes: insufficient training, domain gap, class imbalance")
    print()

    # ── 9. Threshold checks ──
    print("═" * 60)
    print("  VERDICT")
    print("═" * 60)

    checks = []
    checks.append(("C ↔ ONNX stage match (same preproc)", stage_match_central > 0.999))
    checks.append(("C ↔ ONNX RUL diff < 1e-4 (same)",     rul_diff_same.max() < 1e-4))
    checks.append(("C ↔ ONNX SOH diff < 1e-4",            soh_diff.max() < 1e-4))
    checks.append(("CNN Stage accuracy > 70%",            o_stage_acc_c > 0.70))
    checks.append(("CNN RUL MAE < 0.25",                  o_rul_mae_c < 0.25))
    checks.append(("PINN SOH MAE < 0.05",                 c_soh_mae < 0.05))

    all_pass = True
    for desc, ok in checks:
        status = "PASS ✅" if ok else "CHECK ⚠"
        if not ok:
            all_pass = False
        print(f"  {desc:40s}  {status}")

    print()
    if all_pass:
        print("  Overall: ALL CHECKS PASSED ✅")
    else:
        print("  Overall: SOME CHECKS NEED REVIEW ⚠")
    print()
    print("  Note: Gradient computation has been fixed — C now uses central")
    print("  difference (np.gradient), matching the training pipeline.")
    print("  battery_infer.c:89-106, CnnPreprocessor.cpp:computeGradient()")

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
