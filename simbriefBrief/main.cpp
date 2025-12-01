// SimBrief summarizer: reads a SimBrief OFP XML, prints a concise summary, and can optionally
// write a verticalProfile-compatible route CSV.
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

struct Fix {
    std::string name;
    double lat = 0.0;
    double lon = 0.0;
    double altitude_ft = 0.0;
};

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static double deg2rad(double d) { return d * M_PI / 180.0; }

static double haversine_nm(double lat1, double lon1, double lat2, double lon2) {
    const double R_nm = 3440.065;
    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2)) * std::sin(dlon / 2) *
                   std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R_nm * c;
}

static double parse_latlon(const std::string& s) {
    if (s.empty()) return 0.0;
    char hemi = 0;
    std::string num = s;
    if (std::isalpha(static_cast<unsigned char>(s.front()))) {
        hemi = s.front();
        num = s.substr(1);
    } else if (std::isalpha(static_cast<unsigned char>(s.back()))) {
        hemi = s.back();
        num = s.substr(0, s.size() - 1);
    }
    double v = std::stod(num);
    if (hemi == 'S' || hemi == 'W' || hemi == 's' || hemi == 'w') v = -v;
    return v;
}

static std::optional<double> parse_double(const std::string& s) {
    try {
        return std::stod(s);
    } catch (...) {
        return std::nullopt;
    }
}

static std::optional<std::string> tag_in_block(const std::string& block, const std::string& tag) {
    std::regex re("<" + tag + R"(>([^<]+)</)" + tag + ">");
    std::smatch m;
    if (std::regex_search(block, m, re)) return trim(m[1]);
    return std::nullopt;
}

static std::optional<std::string> tag_in_section(const std::string& content,
                                                 const std::string& section_tag,
                                                 const std::string& tag) {
    size_t start = content.find("<" + section_tag + ">");
    size_t end = content.find("</" + section_tag + ">", start);
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        return std::nullopt;
    }
    std::string block = content.substr(start, end - start);
    return tag_in_block(block, tag);
}

struct AircraftInfo {
    std::string name;
    std::string engines;
    std::string reg;
};

static AircraftInfo parse_aircraft(const std::string& content) {
    AircraftInfo info;
    size_t start = content.find("<aircraft>");
    size_t end = content.find("</aircraft>", start);
    if (start != std::string::npos && end != std::string::npos && end > start) {
        std::string block = content.substr(start, end - start);
        if (auto v = tag_in_block(block, "name")) info.name = *v;
        if (auto v = tag_in_block(block, "engines")) info.engines = *v;
        if (auto v = tag_in_block(block, "reg")) info.reg = *v;
    }
    return info;
}

struct FuelInfo {
    std::optional<std::string> ramp;
    std::optional<std::string> trip;
    std::optional<std::string> reserve;
    std::optional<std::string> taxi;
    std::optional<std::string> extra;
};

static FuelInfo parse_fuel(const std::string& content) {
    FuelInfo f;
    size_t start = content.find("<fuel>");
    size_t end = content.find("</fuel>", start);
    if (start != std::string::npos && end != std::string::npos && end > start) {
        std::string block = content.substr(start, end - start);
        if (auto v = tag_in_block(block, "plan_ramp")) f.ramp = v;
        if (auto v = tag_in_block(block, "plan_trip")) f.trip = v;
        if (auto v = tag_in_block(block, "enroute_burn")) f.trip = v;
        if (auto v = tag_in_block(block, "reserve")) f.reserve = v;
        if (auto v = tag_in_block(block, "taxi")) f.taxi = v;
        if (auto v = tag_in_block(block, "extra")) f.extra = v;
    }
    return f;
}

static std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::optional<std::string> tag_value(const std::string& content,
                                            const std::vector<std::string>& tags) {
    for (const auto& t : tags) {
        std::regex re("<" + t + R"(>([^<]+)</)" + t + ">");
        std::smatch m;
        if (std::regex_search(content, m, re)) {
            return trim(m[1]);
        }
    }
    return std::nullopt;
}

static std::vector<Fix> parse_navlog_fixes(const std::string& content) {
    std::vector<Fix> fixes;
    // Extract a <navlog> ... </navlog> block that contains <fix> entries.
    std::string navlog_block;
    size_t search_pos = 0;
    while (true) {
        size_t start = content.find("<navlog>", search_pos);
        if (start == std::string::npos) break;
        size_t end_tag = content.find("</navlog>", start);
        if (end_tag == std::string::npos) break;
        std::string candidate = content.substr(start, end_tag - start);
        if (candidate.find("<fix>") != std::string::npos) {
            navlog_block = candidate;
            break;
        }
        search_pos = end_tag + 9;
    }
    if (navlog_block.empty()) navlog_block = content;

    std::regex block_re("<fix>([\\s\\S]*?)</fix>");
    auto begin = std::sregex_iterator(navlog_block.begin(), navlog_block.end(), block_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string block = (*it)[1];
        std::smatch m;
        Fix f;
        std::regex ident_re("<ident>([^<]+)</ident>");
        if (std::regex_search(block, m, ident_re)) f.name = m[1];
        std::regex lat_re("<pos_lat>([^<]+)</pos_lat>");
        if (std::regex_search(block, m, lat_re)) f.lat = parse_latlon(m[1]);
        std::regex lon_re("<pos_long>([^<]+)</pos_long>");
        if (std::regex_search(block, m, lon_re)) f.lon = parse_latlon(m[1]);
        std::regex alt_re("<altitude_feet>([^<]+)</altitude_feet>");
        if (std::regex_search(block, m, alt_re)) {
            if (auto v = parse_double(m[1])) f.altitude_ft = *v;
        } else {
            std::regex targ_re("<target_altitude>([^<]+)</target_altitude>");
            if (std::regex_search(block, m, targ_re)) {
                if (auto v = parse_double(m[1])) f.altitude_ft = *v;
            }
        }
        if (!f.name.empty()) fixes.push_back(f);
    }
    // Fallback: older navlog_fix attributes.
    if (fixes.empty()) {
        std::regex fix_re(
            R"(<navlog_fix[^>]*fix=\"([^\"]+)\"[^>]*lat=\"([^\"]+)\"[^>]*lon=\"([^\"]+)\"[^>]*alt=\"([^\"]+)\")");
        auto b2 = std::sregex_iterator(content.begin(), content.end(), fix_re);
        for (auto it2 = b2; it2 != end; ++it2) {
            std::smatch m2 = *it2;
            Fix f2;
            f2.name = m2[1];
            f2.lat = parse_latlon(m2[2]);
            f2.lon = parse_latlon(m2[3]);
            f2.altitude_ft = std::stod(m2[4]);
            if (f2.altitude_ft > 0.0 && f2.altitude_ft <= 200.0) f2.altitude_ft *= 100.0;
            fixes.push_back(f2);
        }
    }
    return fixes;
}

static double cumulative_distance(const std::vector<Fix>& fixes) {
    if (fixes.size() < 2) return 0.0;
    double total = 0.0;
    for (size_t i = 1; i < fixes.size(); ++i) {
        total += haversine_nm(fixes[i - 1].lat, fixes[i - 1].lon, fixes[i].lat, fixes[i].lon);
    }
    return total;
}

static void write_route_csv(const std::vector<Fix>& fixes, const std::string& out_path) {
    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << out_path << "\n";
        return;
    }
    out << "# name,distance_nm,altitude_ft\n";
    double cumulative = 0.0;
    if (fixes.empty()) return;
    out << fixes[0].name << "," << cumulative << "," << fixes[0].altitude_ft << "\n";
    for (size_t i = 1; i < fixes.size(); ++i) {
        double leg = haversine_nm(fixes[i - 1].lat, fixes[i - 1].lon, fixes[i].lat, fixes[i].lon);
        cumulative += leg;
        out << fixes[i].name << "," << cumulative << "," << fixes[i].altitude_ft << "\n";
    }
    std::cout << "Route CSV written to " << out_path << " (" << fixes.size() << " fixes)\n";
}

static void print_summary(const std::string& content, const std::vector<Fix>& fixes) {
    auto val = [&](std::vector<std::string> tags, const std::string& fallback = "N/A") {
        auto v = tag_value(content, tags);
        return v ? *v : fallback;
    };
    std::string airline = val({"icao_airline"});
    std::string flight_num = val({"flight_number", "plan_number", "callsign"});
    std::string flight = (airline != "N/A" ? airline + flight_num : flight_num);
    std::string dep = val({"origin", "orig_icao", "icao_code"});
    std::string dep_rwy = tag_in_section(content, "origin", "plan_rwy").value_or(
        val({"origin_rwy", "plan_rwy"}));
    std::string arr = val({"destination", "dest", "dest_icao"});
    std::string arr_rwy =
        tag_in_section(content, "destination", "plan_rwy").value_or(val({"dest_rwy", "arrival_rwy", "plan_rwy"}));
    std::string alt = val({"alternate", "altn", "altn_icao", "altn_code"});
    std::string route = val({"plan_rte", "atc_route", "route", "route_ifps"});
    // In SimBrief XML, initial_altitude often reflects the planned cruise level.
    std::string cruise = val({"initial_altitude", "cruise_altitude", "cruise_fl"});
    std::string airframe = val({"aircraft_icao", "aircraft_type"});
    std::string reg = val({"aircraft_reg"});
    std::string cruise_profile = val({"cruise_profile"});
    std::string distance_plan = val({"route_distance", "gc_distance", "distance"});
    std::string ete = val({"ete", "enroute_time", "block_time"});
    std::string pax = val({"passengers", "pax_count"});
    std::string cargo = val({"cargo"});
    FuelInfo fuel = parse_fuel(content);
    AircraftInfo ac = parse_aircraft(content);
    std::string tow = val({"plan_takeoff", "takeoff_weight"});
    std::string ldw = val({"plan_landing", "landing_weight"});
    std::string zfw = val({"plan_zfw", "zfw", "estimated_zfw"});

    double navlog_dist = cumulative_distance(fixes);

    std::cout << "=== SimBrief Summary ===\n";
    std::cout << "Flight: " << flight << "\n";
    std::cout << "From:   " << dep << (dep_rwy != "N/A" ? " RWY " + dep_rwy : "") << "\n";
    std::cout << "To:     " << arr << (arr_rwy != "N/A" ? " RWY " + arr_rwy : "") << "\n";
    if (alt != "N/A") std::cout << "Alt:    " << alt << "\n";
    std::cout << "Airframe: " << (ac.name.empty() ? airframe : ac.name) << " "
              << (ac.engines.empty() ? "" : "(" + ac.engines + ") ")
              << (ac.reg.empty() ? reg : ac.reg) << "\n";
    if (cruise_profile != "N/A") std::cout << "Cruise profile: " << cruise_profile << "\n";
    std::cout << "Cruise:   " << cruise << " ft\n";
    std::cout << "Route:    " << route << "\n";
    std::cout << "Distance: " << distance_plan << " nm";
    if (navlog_dist > 0.0) std::cout << " (navlog " << static_cast<int>(std::round(navlog_dist)) << " nm)";
    std::cout << "\n";
    std::cout << "ETE:      " << ete << "\n";
    if (pax != "N/A") std::cout << "PAX:      " << pax << "\n";
    if (cargo != "N/A") std::cout << "Cargo:    " << cargo << "\n";
    std::cout << "Fuel (ramp/trip/resv/taxi/extra): "
              << (fuel.ramp ? *fuel.ramp : "N/A") << " / "
              << (fuel.trip ? *fuel.trip : "N/A") << " / "
              << (fuel.reserve ? *fuel.reserve : "N/A") << " / "
              << (fuel.taxi ? *fuel.taxi : "N/A") << " / "
              << (fuel.extra ? *fuel.extra : "N/A") << "\n";
    if (tow != "N/A" || ldw != "N/A" || zfw != "N/A") {
        std::cout << "Weights (TOW/LDW/ZFW): " << tow << " / " << ldw << " / " << zfw << "\n";
    }
    std::cout << "Navlog fixes: " << fixes.size() << "\n";
}

static void usage(const char* prog) {
    std::cout << "Usage: " << prog << " --ofp simbrief_ofp.xml [--csv route.csv]\n";
    std::cout << "Prints a summary of the OFP and optionally writes a route CSV for verticalProfile.\n";
}

int main(int argc, char** argv) {
    try {
        std::string ofp_path;
        std::string csv_out;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--ofp" && i + 1 < argc) {
                ofp_path = argv[++i];
            } else if (arg == "--csv" && i + 1 < argc) {
                csv_out = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                usage(argv[0]);
                return 0;
            } else {
                usage(argv[0]);
                return 1;
            }
        }
        if (ofp_path.empty()) {
            usage(argv[0]);
            return 1;
        }
        auto content = read_file(ofp_path);
        if (!content) {
            std::cerr << "Failed to read OFP file: " << ofp_path << "\n";
            return 1;
        }
        auto fixes = parse_navlog_fixes(*content);
        print_summary(*content, fixes);
        if (!csv_out.empty()) {
            write_route_csv(fixes, csv_out);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
