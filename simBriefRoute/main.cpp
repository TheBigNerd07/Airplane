// SimBrief route extractor: reads a SimBrief OFP XML and writes a verticalProfile-compatible CSV.
#include <cmath>
#include <fstream>
#include <iostream>
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
    // Accepts formats like N47.1234 or 47.1234N or -47.1234
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

static std::vector<Fix> parse_ofp_xml(const std::string& path) {
    std::ifstream file(path);
    std::vector<Fix> fixes;
    if (!file.is_open()) {
        std::cerr << "Failed to open OFP file: " << path << "\n";
        return fixes;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // SimBrief navlog uses tags like <navlog_fix fix="WPT" lat="47.0000" lon="-122.0000" alt="35000" />
    std::regex fix_re(
        R"(<navlog_fix[^>]*fix=\"([^\"]+)\"[^>]*lat=\"([^\"]+)\"[^>]*lon=\"([^\"]+)\"[^>]*alt=\"([^\"]+)\")");
    auto begin = std::sregex_iterator(content.begin(), content.end(), fix_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        Fix f;
        f.name = m[1];
        f.lat = parse_latlon(m[2]);
        f.lon = parse_latlon(m[3]);
        f.altitude_ft = std::stod(m[4]);
        // Some OFPs store altitude in hundreds (e.g., 350 -> FL350). If value looks like a
        // flight level (e.g., <= 200) and not a circuit altitude, scale by 100.
        if (f.altitude_ft > 0.0 && f.altitude_ft <= 200.0) f.altitude_ft *= 100.0;
        fixes.push_back(f);
    }
    return fixes;
}

static void write_route_csv(const std::vector<Fix>& fixes, const std::string& out_path) {
    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << out_path << "\n";
        return;
    }
    out << "# name,distance_nm,altitude_ft\n";
    if (fixes.empty()) return;
    double cumulative = 0.0;
    out << fixes[0].name << "," << cumulative << "," << fixes[0].altitude_ft << "\n";
    for (size_t i = 1; i < fixes.size(); ++i) {
        double leg = haversine_nm(fixes[i - 1].lat, fixes[i - 1].lon, fixes[i].lat, fixes[i].lon);
        cumulative += leg;
        out << fixes[i].name << "," << cumulative << "," << fixes[i].altitude_ft << "\n";
    }
    std::cout << "Wrote " << fixes.size() << " waypoints to " << out_path << "\n";
}

static void usage(const char* prog) {
    std::cout << "Usage: " << prog << " --ofp simbrief_ofp.xml [--out route_sample.csv]\n";
}

int main(int argc, char** argv) {
    std::string ofp_path;
    std::string out_path = "route_sample.csv";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ofp" && i + 1 < argc) {
            ofp_path = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
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
    auto fixes = parse_ofp_xml(ofp_path);
    if (fixes.empty()) {
        std::cerr << "No fixes parsed. Ensure the OFP contains <navlog_fix> entries.\n";
        return 1;
    }
    write_route_csv(fixes, out_path);
    return 0;
}
