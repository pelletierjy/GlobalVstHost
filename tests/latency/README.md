# Hardware Loopback Latency Harness

## T028 — Physical Measurement Setup

This directory contains the **manual** hardware-loopback test harness.
It is **not run in CI** and requires a physical loopback cable.

## Required Hardware

1. **USB audio interface** with duplex (simultaneous input + output) support.
   - Recommended: Focusrite Scarlett 2i2, RME Babyface, or equivalent.
   - Must support all target sample rates: 44.1, 48, 96, 176.4, 192 kHz.

2. **TRS cable** (balanced ¼-inch) connecting:
   - Output 1 (Left)  → Input 1 (Left)
   - Output 2 (Right) → Input 2 (Right)

3. **Reference PC** for CPU profiling gate:
   - Intel Core i5 8th-generation (e.g., i5-8250U) or
   - AMD Ryzen 5 3000-series (e.g., Ryzen 5 3600)
   - 8 GB RAM, Windows 10 1909+ or Windows 11

## Environment Setup

Set the loopback interface friendly name so the harness can detect it:

```powershell
$env:JYGLOBALVST_LOOPBACK_IFACE = "Focusrite USB Audio"
```

## Running the Harness

```powershell
# Build (tests are built automatically when JYGLOBALVST_BUILD_TESTS=ON)
cmake --build build --config Release

# Run
.\build\tests\Release\hardware_loopback_runner.exe
```

Tests that cannot detect the hardware are **skipped** (not failed).

## Scenarios

| Scenario | Description | Gate |
|----------|-------------|------|
| 4 single | One VST3 plugin, 256-sample buffer | Round-trip ≤ 10 ms |
| 4 heavy | 5-plugin chain, 256-sample buffer | Round-trip ≤ 20 ms |
| 8 CPU | 3-plugin chain, 60 s run | Rolling-1s CPU ≤ 5 % |
| 14 soak | Typical chain, 12 hours | 0 xruns, no drift, Δmem ≤ 200 MB |
| 7 rates | 44.1 → 48 → 96 → 176.4 → 192 kHz | No audible artifacts |

## Output

A Markdown report is written to `tests/latency/reports/YYYYMMDD_HHMMSS_hardware_loopback.md`.
