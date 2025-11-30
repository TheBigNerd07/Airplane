# SimBrief Route Exporter (C++)

Reads a SimBrief OFP XML and writes a `route_sample.csv` compatible with `verticalProfile/` (name, cumulative distance nm, altitude ft).

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o simbrief_route
```

## Run
```bash
# Convert a saved SimBrief XML to a route CSV
./simbrief_route --ofp sample_ofp.xml --out route_sample.csv
```

How it works:
- Parses `<navlog_fix>` entries in the SimBrief OFP (`fix`, `lat`, `lon`, `alt` attributes).
- Converts lat/lon to decimal, computes cumulative great-circle distance between fixes, and writes CSV rows with distance and altitude.
- If altitude values are small (e.g., 280), it assumes hundreds and scales to feet.

Input expectations:
- Use SimBrief “XML” OFP download and point `--ofp` to it. The tool looks for `<navlog_fix ...>` elements. If your OFP format differs, adjust `parse_ofp_xml`.
