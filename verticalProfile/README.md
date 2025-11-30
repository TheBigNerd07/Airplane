# Vertical Profile Calculator (C++)

Given a simple route file (waypoint, cumulative distance nm, altitude ft), computes top of climb/descent using climb/descent gradients and draws an ASCII vertical profile.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o vert_profile
```

## Run
```bash
# Using the sample route
./vert_profile --route route_sample.csv

# Custom gradients and more samples for smoother ASCII
./vert_profile --route my_route.csv --climb 350 --descent 280 --samples 300
```

Route CSV format: `name,distance_nm,altitude_ft` where `distance_nm` is cumulative from departure.

What it outputs:
- Total distance, cruise altitude, TOC distance, TOD distance.
- ASCII vertical profile graph of altitude vs distance.
