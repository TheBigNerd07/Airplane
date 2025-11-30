# Flight Log CLI (C++)

Interactive CLI that prompts for a flight and appends it to a CSV log.

## Build
```bash
g++ -std=c++17 -O2 main.cpp -o flight_log
```

## Run
```bash
./flight_log                     # writes to flight_log.csv in this folder
./flight_log --log my_log.csv    # use a different CSV file
```

The tool will:
- Prompt for date, tail, from/to, route, PIC/SIC/night/IFR time, landings (day/night), and remarks.
- Create the CSV with a header if it does not exist.
- Append each new entry as a row.

CSV columns: `date,tail,from,to,route,pic_hours,sic_hours,night_hours,ifr_hours,landings_day,landings_night,remarks`
