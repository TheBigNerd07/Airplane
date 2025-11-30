# NOTAM Risk CLI (C++)

Fetch or load NOTAMs for an ICAO, flag key hazards (runway closure, approach outages, GPS unreliability, lighting issues), and compute a simple risk score.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o notam_risk
```

## Run
```bash
# Parse a saved NOTAM text file
./notam_risk --icao KJFK --file sample_notams.txt

# Try live fetch via curl (network required)
./notam_risk --icao KJFK

# Only print the risk score
./notam_risk --icao KJFK --file sample_notams.txt --risk-only
```

What it does:
- Reads NOTAMs from a file (recommended) or fetches via `curl`.
- Parses each NOTAM line and flags: runway closures, approach/NAVAID outages, GPS unreliability, runway/approach lighting issues.
- Computes a simple additive risk score (runway closure +4, approach outage +3, GPS +2, lighting +1 per NOTAM) and lists reasons.

Inputs:
- `--icao <ICAO>` (required)
- `--file <path>` to use local NOTAM text (one NOTAM per line works; raw FAA text also works).
- `--risk-only` to suppress listing and just output the score.

Note: Live fetch uses `curl` against FAA NOTAM query; if offline, use `--file` with saved NOTAM text. Adjust patterns in `parse_notams_text` for your provider’s format.
