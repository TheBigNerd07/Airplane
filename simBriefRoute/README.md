# SimBrief Route Exporter (C++)

Reads a SimBrief OFP XML, prints a concise summary, and can optionally write a `route_sample.csv` compatible with `verticalProfile/` (name, cumulative distance nm, altitude ft).

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o simbrief_route
```

## Run
```bash
# Summarize a saved SimBrief XML
./simbrief_route --ofp sample_ofp.xml

# Summarize and export route CSV
./simbrief_route --ofp sample_ofp.xml --csv route_sample.csv
```

What it does:
- Prints key OFP fields when present (flight number/callsign, origin/dest/alt, route string, cruise altitude/FL, distance, ETE, fuel plan, pax/cargo, airframe).
- Parses `<navlog_fix>` entries (`fix`, `lat`, `lon`, `alt`), computes great-circle cumulative distance, and reports fix count.
- If `--csv` is provided, also writes a verticalProfile-friendly CSV with cumulative distances and altitudes (scales flight levels like 350 -> 35000).

Input expectations:
- Use SimBrief “XML” OFP download and point `--ofp` to it. The tool looks for `<navlog_fix ...>` elements and common tags like `origin`, `destination`, `plan_rte`, `cruise_altitude`, `fuel_plan_*`, etc. If your OFP schema differs, tweak tag names in `main.cpp`.
