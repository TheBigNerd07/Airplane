# Aviation Weather Decoder (C++)

Small, self-contained CLI that takes a raw METAR (and optional TAF), decodes key elements, and flags them against personal minima.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o wx_brief
```

## Run
Pass a METAR string (quoted) and optionally a TAF. Set personal minima and runway heading for crosswind checks.
```bash
./wx_brief --metar "KJFK 011651Z 18012G18KT 10SM BKN025 OVC035 18/12 A2992" \
           --runway 220 \
           --min-ceiling 1000 \
           --min-vis 3 \
           --max-xwind 15
```

Output shows wind with headwind/crosswind components, visibility, ceiling, and significant weather vs the minima. If you also want to keep the TAF alongside the METAR, add `--taf "RAW TAF STRING"` (it is displayed raw).
