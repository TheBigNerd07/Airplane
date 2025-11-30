# Aviation Weather Decoder (C++)

Small, self-contained CLI that takes a raw METAR (and optional TAF), decodes key elements, and flags them against personal minima. It can also fetch METARs live by ICAO, compare multiple reports for trends, and output JSON.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o wx_brief
```

## Run
Pass one or more METAR strings (quoted) or ask the tool to fetch by ICAO. Set personal minima and runway heading for crosswind checks.
```bash
# Manual input, text output
./wx_brief --metar "KJFK 011651Z 18012G18KT 10SM BKN025 OVC035 18/12 A2992" \
           --runway 220 --min-ceiling 1000 --min-vis 3 --max-xwind 15

# Fetch latest live METAR by ICAO, output JSON
./wx_brief --icao kjfk --format json --runway 220

# Fetch last 5 METARs for trend automatically (from NOAA cycles)
./wx_brief --icao kjfk --icao-history 5 --runway 220

# Compare trend across multiple reports (oldest -> latest)
./wx_brief --metar "KJFK 011651Z 18012KT 10SM SCT025 15/10 A2992" \
           --metar "KJFK 011851Z 19018G25KT 4SM -RA OVC015 16/12 A2986" \
           --runway 220
```

What you get:
- Wind with headwind/crosswind components vs your runway and max crosswind.
- Visibility and ceiling vs minima.
- Plain-English weather tags (rain/snow/fog/etc).
- Trend summary if you pass more than one METAR.
- Optional raw TAF display with `--taf "RAW TAF STRING"`.

Output formats:
- `--format text` (default) prints human-friendly sections.
- `--format json` prints a JSON object with `metars` array, optional `trend`, and optional `taf_raw`.

Notes:
- Live fetch hits `https://tgftp.nws.noaa.gov/data/observations/metar/stations/<ICAO>.TXT` via `curl`; network access must be available and `curl` installed.
- History fetch uses hourly cycle files `https://tgftp.nws.noaa.gov/data/observations/metar/cycles/<HH>Z.TXT` to pull the last N reports for the ICAO (up to the past ~48 hours).
