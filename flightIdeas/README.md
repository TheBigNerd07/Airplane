Route suggester for your simulator fleet. Provide a simple `aircraft.csv`, ship a small `airports.csv`, and get route ideas that fit range and runway needs.

## Files
- `main.cpp` – C++17 CLI.
- `aircraft.csv` – sample fleet list (edit to your aircraft).
- `airports.csv` – small airport dataset; add more rows as needed.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o route_suggester
```

## Run
```bash
# Basic: use bundled CSVs, 3 suggestions per aircraft
./route_suggester

# Limit to a region (country or region code)
./route_suggester --region US-WA

# Random starts (ignore home airport, pick a random departure)
./route_suggester --random-start

# Use your own CSV paths and different count
./route_suggester --aircraft my_fleet.csv --airports my_airports.csv --count 5
```

### CSV formats
- `aircraft.csv` columns: `name,role,range_nm[,min_runway_ft,home]`
  - `home` optional; leave blank (as in the sample) to allow random starts, or set one if you want home-based suggestions without `--random-start`.
  - `min_runway_ft` optional; if blank, inferred from role (GA ~2500, turboprop ~4000, regional ~5500, jet ~6500, widebody ~8000).
  - Example: `KingAir,Turboprop,1200,0,KBFI`
- `airports.csv` columns: `icao,name,country,region,lat,lon,longest_runway_ft[,kind]`

## How suggestions work
- Picks destinations that meet runway length and (if home airport is known) fall between ~30–90% of aircraft range.
- Falls back to any runway-qualified airport if no range match.
- Randomizes the list, returns `--count` routes per aircraft.

## Next ideas
- Expand airports.csv with more regions.
- Add exclusions (avoid repeats, avoid certain airports), and JSON output if you want to script it.
