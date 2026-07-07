#!/usr/bin/env python3
"""===========================================================================
 * generate_demo_data.py — LiFePO4 18650 Demo Data Generator
 *
 * Generates a binary file for FileDataProvider replay.
 * Format: [ic_curve: 128×float32] + [features: 132×float32] per frame
 * Frame size: 260 floats = 1040 bytes
 *
 * Simulates a realistic 18650 LiFePO4 (3.2V nominal) battery aging test:
 *   - IC curve: characteristic sharp double-peak (LFP phase transitions)
 *   - SOH: linear decay 1.0 → 0.65 across 900 cycles (~3 min demo at 10Hz)
 *   - Temperature: random walk 25-45°C with occasional thermal events
 *   - Capacity: proportional to SOH
 *   - Cell swelling: rare random spikes (2% probability)
 *
 * Usage:
 *   python3 generate_demo_data.py [--frames N] [--output path]
 *
 *   Default: 900 frames → demo_data_18650_lfp.bin (936 KB)
 *
 * Then test with:
 *   ./battery_hmi --file demo_data_18650_lfp.bin
 *==========================================================================="""

import argparse
import struct
import numpy as np


# ══════════════════════════════════════════════════════════════════════════════
# 18650 LiFePO4 Battery Parameters
# ══════════════════════════════════════════════════════════════════════════════

NOMINAL_V = 3.2        # V — LiFePO4 nominal
CHARGE_V  = 3.65       # V — full charge cutoff
CUTOFF_V  = 2.5        # V — discharge cutoff
NOMINAL_MAH = 2000.0   # mAh — typical 18650 LFP

IC_POINTS = 128        # voltage grid points
FEAT_POINTS = 132       # total feature vector length

# Voltage grid for IC curve (128 points, 2.5V → 3.65V)
VOLTAGE_GRID = np.linspace(CUTOFF_V, CHARGE_V, IC_POINTS)

# LFP characteristic peak positions in volts
#   Main peak (FePO4 ↔ LiFePO4 phase transition): ~3.40V during charge
#   Secondary peak (solid-solution region): ~3.33V
PEAK_MAIN_V = 3.40       # primary FePO4/LiFePO4 transition
PEAK_SEC_V  = 3.33       # secondary solid-solution transition


# ══════════════════════════════════════════════════════════════════════════════
# IC Curve Generation — LiFePO4 Double-Peak Model
# ══════════════════════════════════════════════════════════════════════════════

def make_ic_curve(soh: float, rng: np.random.Generator) -> np.ndarray:
    """
    Generate a realistic LiFePO4 IC curve (dQ/dV vs V).

    LFP has very sharp peaks due to the flat OCV plateau.
    We use two Gaussians:
      - Main peak at ~3.40V (FePO4/LiFePO4 transition), very sharp (σ ~8mV)
      - Secondary peak at ~3.33V (solid-solution), broader (σ ~20mV)

    Peak heights scale with SOH. Positions shift slightly right as battery ages.
    """
    # Peak height: LFP peaks are tall and narrow
    #   Fresh cell: main peak ~8-10 dQ/dV units
    #   Aged cell (SOH=0.7): ~5-6 units
    main_height = 9.0 * soh + 1.0
    sec_height  = 2.5 * soh + 0.3

    # Peak width (sigma in volts): LFP is very sharp
    #   Fresh: σ ≈ 0.008V (FWHM ≈ 19mV)
    #   Aged:  σ ≈ 0.012V (FWHM ≈ 28mV) — slight broadening
    main_sigma = 0.008 + (1.0 - soh) * 0.010
    sec_sigma  = 0.020 + (1.0 - soh) * 0.015

    # Peak position: drifts right (higher voltage) as battery ages
    #   Fresh: main @ 3.400V, sec @ 3.330V
    #   Aged:  main @ 3.415V, sec @ 3.345V
    main_pos = PEAK_MAIN_V + (1.0 - soh) * 0.030
    sec_pos  = PEAK_SEC_V  + (1.0 - soh) * 0.030

    # Build IC curve
    ic = np.zeros(IC_POINTS, dtype=np.float32)

    # Main peak (Gaussian)
    ic += main_height * np.exp(-0.5 * ((VOLTAGE_GRID - main_pos) / main_sigma)**2)

    # Secondary peak (Gaussian)
    ic += sec_height * np.exp(-0.5 * ((VOLTAGE_GRID - sec_pos) / sec_sigma)**2)

    # Small baseline ripple (minute oscillations mimicking measurement noise)
    ic += 0.08 * np.sin(VOLTAGE_GRID * 25.0)

    # Background dQ/dV (tiny constant from side reactions)
    ic += 0.05

    # Measurement noise
    ic += rng.normal(0.0, 0.04, IC_POINTS)

    # Ensure non-negative
    ic = np.maximum(ic, 0.0)

    return ic.astype(np.float32)


# ══════════════════════════════════════════════════════════════════════════════
# Main Generation
# ══════════════════════════════════════════════════════════════════════════════

def generate_frames(num_frames: int, seed: int = 42) -> list:
    """
    Generate num_frames of BatterySample-compatible binary data.

    Returns list of (ic_curve, features) tuples as numpy float32 arrays.
    """
    rng = np.random.default_rng(seed)

    frames = []

    # State variables
    temperature = 28.0       # start at room temp
    soh = 1.0                # start fresh
    cycle_count = 0
    cell_swelling = 0.0

    # SOH decay: 1.0 → 0.65 over num_frames
    #  Each frame ≈ one read at 10Hz. For num_frames=900:
    #    900 frames × 100ms = 90 seconds of real-time demo
    #    Simulating ~900 cycles of aging
    soh_decay_per_frame = 0.35 / num_frames  # total decay / total frames

    for tick in range(num_frames):
        # ── SOH decay ──
        soh = 1.0 - soh_decay_per_frame * tick
        # Add tiny random jitter so it's not perfectly linear
        soh += rng.normal(0.0, 0.002)
        soh = max(0.65, min(1.0, soh))

        # ── IC curve ──
        ic_curve = make_ic_curve(soh, rng)

        # ── Temperature random walk (25–45°C) ──
        temperature += rng.normal(0.0, 0.08)
        # Occasional thermal events (e.g., high-rate discharge)
        if rng.random() < 0.005:  # 0.5% chance per frame
            temperature += rng.uniform(2.0, 8.0)
        temperature = max(25.0, min(45.0, temperature))

        # ── Features vector ──
        features = np.zeros(FEAT_POINTS, dtype=np.float32)

        # features[0:128] = IC curve
        features[0:128] = ic_curve

        # features[128] = temperature (°C)
        features[128] = temperature

        # features[129] = log10(cycle_count + 1)
        features[129] = np.log10(float(cycle_count) + 1.0)

        # features[130] = dV/dt proxy (voltage drop rate, grows with aging)
        features[130] = -0.03 - (1.0 - soh) * 0.08 + rng.normal(0.0, 0.002)

        # features[131] = capacity (normalized to nominal)
        capacity = NOMINAL_MAH * (0.85 + 0.15 * soh)
        features[131] = capacity / NOMINAL_MAH

        # ── Cycle count: increment every ~10 frames (simulating ~1 cycle/minute) ──
        if tick > 0 and tick % 10 == 0:
            cycle_count += 1

        # ── Cell swelling: rare spikes ──
        if rng.random() < 0.02:
            cell_swelling = rng.uniform(0.1, 0.5)
        else:
            cell_swelling = max(0.0, cell_swelling - 0.03)

        frames.append((ic_curve, features))

    return frames


def write_binary(frames: list, output_path: str):
    """Write frames to binary file in FileDataProvider-compatible format."""
    with open(output_path, 'wb') as f:
        for ic_curve, features in frames:
            f.write(ic_curve.tobytes())    # 128 × float32 = 512 bytes
            f.write(features.tobytes())    # 132 × float32 = 528 bytes
            # Total: 1040 bytes per frame

    file_size = len(frames) * 1040
    print(f"Written {len(frames)} frames ({file_size:,} bytes) → {output_path}")
    print(f"  IC curve range:  [{VOLTAGE_GRID[0]:.2f}V – {VOLTAGE_GRID[-1]:.2f}V] × {IC_POINTS} points")
    print(f"  SOH range:       1.00 → 0.65")
    print(f"  Duration at 10Hz: {len(frames) * 0.1:.0f}s")
    print(f"  Chemistry:        LiFePO4 (LFP) — 18650 — {NOMINAL_V}V nominal")


# ══════════════════════════════════════════════════════════════════════════════
# CLI
# ══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="Generate LiFePO4 18650 demo data for battery_hmi"
    )
    parser.add_argument(
        "--frames", "-n", type=int, default=900,
        help="Number of frames to generate (default: 900, ~90s demo at 10Hz)"
    )
    parser.add_argument(
        "--output", "-o", type=str,
        default="demo_data_18650_lfp.bin",
        help="Output file path (default: demo_data_18650_lfp.bin)"
    )
    parser.add_argument(
        "--seed", "-s", type=int, default=42,
        help="Random seed (default: 42)"
    )
    args = parser.parse_args()

    print("═" * 60)
    print("  LiFePO4 18650 Demo Data Generator")
    print("═" * 60)
    print(f"  Frames:     {args.frames}")
    print(f"  Output:     {args.output}")
    print(f"  Seed:       {args.seed}")
    print(f"  Chemistry:  LiFePO4 (LFP) 18650 — {NOMINAL_V}V nominal")
    print(f"  Grid:       {CUTOFF_V}V – {CHARGE_V}V, {IC_POINTS} points")
    print("═" * 60)

    frames = generate_frames(args.frames, seed=args.seed)
    write_binary(frames, args.output)

    print("\nRun with:")
    print(f"  ./battery_hmi --file {args.output}")


if __name__ == "__main__":
    main()
