# Flight Suite Launcher (TUI)

Lightweight text UI that wraps the other tools in this repo. It launches the built binaries from sibling folders.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o flight_suite
```

## Run
```bash
./flight_suite
```

Menu options:
- METAR Decoder (`metarViewer/wx_brief`)
- Route Suggester (`flightIdeas/route_suggester`)
- NOTAM Risk (`notamTool/notam_risk`)
- E6B Calculator (`e6bTool/e6b`)
- Vertical Profile (`verticalProfile/vert_profile`)
- SimBrief Summary / Route -> CSV (`simBriefRoute/simbrief_route`)

Notes:
- Build each tool first in its own folder; the launcher expects binaries at the relative paths above.
- This is a text UI (no graphics) to keep dependencies minimal. It prompts for the same inputs each tool expects and prints their output.
- METAR menu supports fetching multiple recent reports when you enter a history count (uses `--icao-history`).
