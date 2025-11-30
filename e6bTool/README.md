# E6B Flight Computer (C++)

CLI with common E6B calculations.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o e6b
```

## Run (examples)
```bash
# Wind triangle: heading 090, TAS 120, wind 030@20
./e6b winds 90 120 30 20

# Crosswind/headwind on runway 22 with wind 190@15
./e6b xwind 190 15 220
./e6b headwind 190 15 220

# Pressure / density altitude at 1500 ft, altimeter 30.12, OAT 20C
./e6b density_alt 1500 30.12 20

# Mach/TAS conversions at -20C
./e6b mach 450 -20
./e6b tas 0.78 -20

# Time-speed-distance: 120 nm at 135 kt
./e6b tsd 120 135

# Fuel burn: 12 gph for 2.5 hr
./e6b fuel 12 2.5
```

Modes:
- `winds <hdg_deg> <tas_kt> <wind_dir_deg> <wind_spd_kt>` → GS, resulting track, wind correction angle
- `xwind <wind_dir_deg> <wind_spd_kt> <runway_deg>`
- `headwind <wind_dir_deg> <wind_spd_kt> <runway_deg>`
- `pressure_alt <field_elev_ft> <altimeter_inhg>`
- `density_alt <field_elev_ft> <altimeter_inhg> <oat_c>`
- `mach <tas_kt> <oat_c>`
- `tas <mach> <oat_c>`
- `tsd <distance_nm> <groundspeed_kt>` (time in minutes)
- `fuel <flow_gph> <time_hr>`
- `drift <wind_dir_deg> <wind_spd_kt> <tas_kt> <track_deg>`
- `groundspeed <tas_kt> <wind_component_kt>`
