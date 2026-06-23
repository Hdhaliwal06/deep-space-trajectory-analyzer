# Deep Space Trajectory Analyzer


This version is intentionally simple:

- One C source file
- Two local data files
- No network calls required
- Compile with `make`
- Query Voyager 1 or Voyager 2 by any date

## Build

```bash
make
```

## Run

```bash
./voyager voyager1 2024-Jan-01
./voyager voyager1 2024-Mar-15
./voyager voyager1 2025-Jan-01
./voyager voyager2 2024-Jun-01
```

## What It Calculates

The program reads:

- Position: `x`, `y`, `z` in kilometers
- Velocity: `vx`, `vy`, `vz` in kilometers per second

Then it computes:

- Distance from the Sun in kilometers
- Distance from the Sun in astronomical units
- Speed relative to the Sun
- One-way light travel time based on that distance

## Data Format

The files in `data/` use a tiny HORIZONS-style structure:

```text
$$SOE
2024-Jan-01, 21130000000, -11900000000, 920000000, 10.20, -7.15, 0.18
$$EOE
```

Columns:

```text
date, x_km, y_km, z_km, vx_km_s, vy_km_s, vz_km_s
```

The included data is a small learning sample shaped like NASA HORIZONS vector output. The program interpolates between the sample rows when your date falls inside the file range, and estimates beyond the range using the nearest record's velocity.

To make the project more resume-ready, replace these rows with a fuller HORIZONS export for Voyager 1 and Voyager 2.
