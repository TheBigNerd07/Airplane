// Route suggester: reads aircraft.csv and airports.csv, and proposes routes suited to each airframe.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Airport {
    std::string icao;
    std::string name;
    std::string country;
    std::string region;
    double lat = 0.0;
    double lon = 0.0;
    int longest_runway_ft = 0;
    std::string kind;
};

struct Aircraft {
    std::string name;
    std::string role;
    std::string home;
    double range_nm = 500.0;
    int min_runway_ft = 0;
};

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> cells;
    std::string cell;
    std::istringstream ss(line);
    while (std::getline(ss, cell, ',')) {
        cells.push_back(trim(cell));
    }
    return cells;
}

static bool read_file_lines(const std::string& path, std::vector<std::string>& lines_out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) lines_out.push_back(line);
    }
    return true;
}

static std::vector<Airport> load_airports(const std::string& path) {
    std::vector<Airport> airports;
    std::vector<std::string> lines;
    if (!read_file_lines(path, lines)) {
        std::cerr << "Failed to open airports file: " << path << "\n";
        return airports;
    }
    for (const auto& line : lines) {
        if (line.empty() || line[0] == '#') continue;
        auto cells = split_csv_line(line);
        if (cells.size() < 7) continue;
        Airport a;
        a.icao = cells[0];
        a.name = cells[1];
        a.country = cells[2];
        a.region = cells[3];
        a.lat = std::stod(cells[4]);
        a.lon = std::stod(cells[5]);
        a.longest_runway_ft = std::stoi(cells[6]);
        a.kind = cells.size() > 7 ? cells[7] : "";
        airports.push_back(a);
    }
    return airports;
}

static std::vector<Aircraft> load_aircraft(const std::string& path) {
    std::vector<Aircraft> planes;
    std::vector<std::string> lines;
    if (!read_file_lines(path, lines)) {
        std::cerr << "Failed to open aircraft file: " << path << "\n";
        return planes;
    }
    for (const auto& line : lines) {
        if (line.empty() || line[0] == '#') continue;
        auto cells = split_csv_line(line);
        if (cells.size() < 3) continue;
        Aircraft ac;
        ac.name = cells[0];
        ac.role = cells[1];
        ac.home = cells[2];
        if (cells.size() > 3 && !cells[3].empty()) {
            ac.range_nm = std::stod(cells[3]);
        }
        if (cells.size() > 4 && !cells[4].empty()) {
            ac.min_runway_ft = std::stoi(cells[4]);
        }
        planes.push_back(ac);
    }
    return planes;
}

static int role_min_runway(const std::string& role_raw) {
    std::string r = role_raw;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    if (r.find("wide") != std::string::npos || r.find("long") != std::string::npos) return 8000;
    if (r.find("jet") != std::string::npos || r.find("737") != std::string::npos ||
        r.find("320") != std::string::npos)
        return 6500;
    if (r.find("regional") != std::string::npos || r.find("crj") != std::string::npos ||
        r.find("e175") != std::string::npos)
        return 5500;
    if (r.find("turboprop") != std::string::npos || r.find("king") != std::string::npos ||
        r.find("pc-12") != std::string::npos)
        return 4000;
    if (r.find("ga") != std::string::npos || r.find("piston") != std::string::npos ||
        r.find("172") != std::string::npos || r.find("pa-") != std::string::npos)
        return 2500;
    return 3500;
}

static double deg2rad(double d) { return d * M_PI / 180.0; }

static double haversine_nm(double lat1, double lon1, double lat2, double lon2) {
    const double R_nm = 3440.065; // nautical miles
    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2)) * std::sin(dlon / 2) *
                   std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R_nm * c;
}

struct Suggestion {
    std::string from_icao;
    std::string to_icao;
    double distance_nm = 0.0;
};

static const Airport* pick_random_airport(const std::vector<Airport>& airports, int min_rwy,
                                          const std::string& region_filter, std::mt19937& gen) {
    std::vector<const Airport*> candidates;
    for (const auto& a : airports) {
        if (a.longest_runway_ft < min_rwy) continue;
        if (!region_filter.empty() && a.country != region_filter && a.region != region_filter)
            continue;
        candidates.push_back(&a);
    }
    if (candidates.empty()) return nullptr;
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return candidates[dist(gen)];
}

static std::vector<Suggestion> suggest_routes(const Aircraft& ac,
                                              const std::unordered_map<std::string, Airport>& by_icao,
                                              const std::vector<Airport>& airports, int count,
                                              const std::string& region_filter, bool random_start) {
    std::vector<Suggestion> out;
    int min_rwy = ac.min_runway_ft > 0 ? ac.min_runway_ft : role_min_runway(ac.role);
    double min_leg = ac.range_nm * 0.3;
    double max_leg = ac.range_nm * 0.9;
    std::random_device rd;
    std::mt19937 gen(rd());

    auto home_it = by_icao.find(ac.home);
    const Airport* home = (random_start || home_it == by_icao.end()) ? nullptr : &home_it->second;
    if (!home) {
        home = pick_random_airport(airports, min_rwy, region_filter, gen);
    }

    std::vector<std::pair<const Airport*, double>> candidates;
    for (const auto& a : airports) {
        if (home && a.icao == home->icao) continue;
        if (a.longest_runway_ft < min_rwy) continue;
        if (!region_filter.empty() && a.country != region_filter && a.region != region_filter)
            continue;
        double dist = 0.0;
        if (home) {
            dist = haversine_nm(home->lat, home->lon, a.lat, a.lon);
            if (dist < min_leg || dist > max_leg) continue;
        }
        candidates.push_back({&a, dist});
    }

    if (candidates.empty()) {
        // Relax distance filter if nothing found
        for (const auto& a : airports) {
            if (a.icao == ac.home) continue;
            if (a.longest_runway_ft < min_rwy) continue;
            if (!region_filter.empty() && a.country != region_filter && a.region != region_filter)
                continue;
            double dist = 0.0;
            if (home) dist = haversine_nm(home->lat, home->lon, a.lat, a.lon);
            candidates.push_back({&a, dist});
        }
    }

    std::shuffle(candidates.begin(), candidates.end(), gen);

    for (size_t i = 0; i < candidates.size() && (int)out.size() < count; ++i) {
        Suggestion s;
        s.from_icao = home ? home->icao : "N/A";
        s.to_icao = candidates[i].first->icao;
        s.distance_nm = candidates[i].second;
        out.push_back(s);
    }
    return out;
}

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [--aircraft aircraft.csv] [--airports airports.csv] [--count 3] "
                 "[--region USA|US-WA|...] [--random-start]\n";
    std::cerr << " aircraft.csv columns: name,role,home,range_nm[,min_runway_ft]\n";
    std::cerr << " airports.csv columns: icao,name,country,region,lat,lon,longest_runway_ft[,kind]\n";
}

int main(int argc, char** argv) {
    std::string aircraft_path = "aircraft.csv";
    std::string airports_path = "airports.csv";
    std::string region_filter;
    int count = 3;
    bool random_start = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--aircraft" && i + 1 < argc) {
            aircraft_path = argv[++i];
        } else if (arg == "--airports" && i + 1 < argc) {
            airports_path = argv[++i];
        } else if (arg == "--count" && i + 1 < argc) {
            count = std::stoi(argv[++i]);
        } else if (arg == "--region" && i + 1 < argc) {
            region_filter = argv[++i];
        } else if (arg == "--random-start") {
            random_start = true;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    auto airports = load_airports(airports_path);
    if (airports.empty()) {
        std::cerr << "No airports loaded.\n";
        return 1;
    }
    std::unordered_map<std::string, Airport> by_icao;
    for (const auto& a : airports) {
        by_icao[a.icao] = a;
    }

    auto aircraft = load_aircraft(aircraft_path);
    if (aircraft.empty()) {
        std::cerr << "No aircraft loaded.\n";
        return 1;
    }

    for (const auto& ac : aircraft) {
        std::cout << "=== " << ac.name << " (" << ac.role << "), home " << ac.home
                  << ", range " << ac.range_nm << "nm"
                  << ", min rwy "
                  << (ac.min_runway_ft > 0 ? ac.min_runway_ft : role_min_runway(ac.role))
                  << " ft ===\n";
        auto routes = suggest_routes(ac, by_icao, airports, count, region_filter, random_start);
        if (routes.empty()) {
            std::cout << "No suggestions found.\n";
            continue;
        }
        for (size_t i = 0; i < routes.size(); ++i) {
            const auto& r = routes[i];
            std::cout << "  " << (i + 1) << ") " << r.from_icao << " -> " << r.to_icao;
            if (r.distance_nm > 0.0) {
                std::cout << " (" << static_cast<int>(std::round(r.distance_nm)) << " nm)";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    return 0;
}
