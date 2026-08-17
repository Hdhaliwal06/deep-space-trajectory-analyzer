# Deep Space Trajectory Analyzer

A C project that reads NASA HORIZONS-style Voyager ephemeris files and prints a mission status report.

## Build

```bash
make
```

## Update Data

Refresh the local Voyager ephemeris cache from NASA JPL HORIZONS:

```bash
make update-data
```

The updater writes:

- `data/voyager1.txt`
- `data/voyager2.txt`

The analyzer itself stays offline-friendly: it reads the latest local files instead of calling NASA every time it runs.

## Run

```bash
./voyager voyager1 2024-Jan-01
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

## Data Source

The `scripts/update_data.py` script queries the NASA JPL HORIZONS API for heliocentric Voyager state vectors:

- Voyager 1: `COMMAND='-31'`
- Voyager 2: `COMMAND='-32'`
- Center: Sun, `CENTER='500@10'`
- Ephemeris type: `VECTORS`
- Units: `KM-S`
- Step size: `30 d`

## Data Format

The files in `data/` use a small HORIZONS-style structure:

```text
$$SOE
2024-Jan-01, 21130000000, -11900000000, 920000000, 10.20, -7.15, 0.18
$$EOE
```

Columns:

```text
date, x_km, y_km, z_km, vx_km_s, vy_km_s, vz_km_s
```

The included data is generated from NASA HORIZONS vector output. The program interpolates between rows when your date falls inside the file range, and estimates beyond the range using the nearest record's velocity.
