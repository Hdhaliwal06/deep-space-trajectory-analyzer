# Deep Space Trajectory Analyzer

An educational C analyzer for Voyager heliocentric state vectors from NASA JPL HORIZONS. It reports distance from the Sun, speed, and one-way light time for an exact, interpolated, or estimated date. A small Python standard-library web server exposes the same C calculation through a browser UI.

## Run

```bash
make
./build/voyager voyager1 2024-Jan-01
./build/voyager voyager2 2024-Jun-01 --json
make web
```

Then open `http://127.0.0.1:8000`. The web UI invokes the compiled C analyzer; it does not duplicate its calculations.

If port 8000 is already in use, choose another local port:

```bash
PORT=8080 make web
```

Open the matching address, such as `http://127.0.0.1:8080`. Stop the server with `Ctrl-C`.

## Data pipeline

`scripts/update_data.py` requests NASA JPL HORIZONS vectors for Voyager 1 (`-31`) and Voyager 2 (`-32`) relative to the Sun (`CENTER='500@10'`), in km and km/s. It stores a compact local cache in:

- `data/Voyager1.txt`
- `data/Voyager2.txt`

Refresh the cache with `make update-data`. The analyzer stays offline at runtime for reproducibility and predictable behavior.

## Methods and limits

- Exact cached date: returns the source record.
- Date between cached records: linearly interpolates position and velocity components.
- Date outside the cache: extrapolates from the nearest record at constant velocity and prints an explicit warning.

Distance and speed are Euclidean magnitudes of the three-dimensional position and velocity vectors. AU uses 149,597,870.7 km; light time uses 299,792.458 km/s.

This is not flight-navigation software. Linear interpolation and especially constant-velocity extrapolation omit gravitational perturbations, maneuvers, and uncertainty modeling. For navigation or high-precision science, query an authoritative ephemeris at the requested time or use a validated astrodynamics propagator.

## Quality checks

```bash
make test
```

Tests cover exact lookup, interpolation, extrapolation, invalid spacecraft selection, and invalid calendar dates. The C parser also rejects out-of-order ephemeris records.

## Project structure

```text
main.c                 C parsing, date resolution, and calculations
scripts/update_data.py NASA HORIZONS cache updater
scripts/web.py         local API bridge to the compiled C program
web/                   browser interface
tests/                 end-to-end analyzer tests
```
