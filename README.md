# Airplane Projects

Two small C++ CLI tools:

- `metarViewer/`: Aviation weather decoder. Takes raw METAR (or fetches live by ICAO), computes crosswind/headwind vs your runway, checks visibility/ceiling against personal minima, trends across multiple reports, and can output text or JSON.
- `flightIdeas/`: Route suggester. Reads your fleet list (`aircraft.csv`) and a small airport list (`airports.csv`) and proposes routes suited to each airframe (range/runway/region). Supports random departures, region filters, and sample data you can edit.

See each subfolder’s README for build/run details. Both build with `g++ -std=c++17`. 
