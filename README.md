# Airplane Projects

Seven small C++ CLI tools + a launcher:

- `metarViewer/`: Aviation weather decoder. Takes raw METAR (or fetches live by ICAO), computes crosswind/headwind vs your runway, checks visibility/ceiling against personal minima, trends across multiple reports, and can output text or JSON.
- `flightIdeas/`: Route suggester. Reads your fleet list (`aircraft.csv`) and a small airport list (`airports.csv`) and proposes routes suited to each airframe (range/runway/region). Supports random departures, region filters, and sample data you can edit.
- `flightLog/`: Flight log updater. Prompts for flight details and appends them to a CSV (auto-creates with headers).
- `notamTool/`: NOTAM risk checker. Loads or fetches NOTAMs for an ICAO, flags closures/approach/GPS/lighting issues, and computes a simple risk score.
- `verticalProfile/`: Vertical profile calculator. Reads a route with cumulative distance/altitudes, computes TOC/TOD using climb/descent gradients, and renders an ASCII altitude profile.
- `e6bTool/`: E6B flight computer. Provides wind triangle, crosswind/headwind, pressure/density altitude, Mach/TAS conversions, TSD, fuel burn, drift, and related calculations.
- `simBriefRoute/`: SimBrief route exporter. Converts a SimBrief OFP XML into a `route_sample.csv` for verticalProfile (cumulative distance and altitude).
- `flightSuiteGUI/`: Text UI launcher that wraps the tools above; provides a menu to run each binary with prompts.

See each subfolder’s README for build/run details. All build with `g++ -std=c++17`. 
