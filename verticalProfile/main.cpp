// Vertical Profile Calculator: reads a route file (waypoint, distance_nm, altitude_ft),
// computes TOC/TOD based on climb/descent gradients, and renders an ASCII profile.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Waypoint {
    std::string name;
    double distance_nm = 0.0; // cumulative distance from origin
    double altitude_ft = 0.0;
};

struct ProfilePoints {
    std::vector<double> distances_nm;
    std::vector<double> altitudes_ft;
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

static std::vector<Waypoint> load_route(const std::string& path) {
    std::vector<Waypoint> wpts;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open route file: " << path << "\n";
        return wpts;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto cells = split_csv_line(line);
        if (cells.size() < 3) continue;
        Waypoint w;
        w.name = cells[0];
        w.distance_nm = std::stod(cells[1]);
        w.altitude_ft = std::stod(cells[2]);
        wpts.push_back(w);
    }
    return wpts;
}

static ProfilePoints interpolate_profile(const std::vector<Waypoint>& wpts, int samples = 200) {
    ProfilePoints p;
    if (wpts.size() < 2) return p;
    double total_dist = wpts.back().distance_nm;
    for (int i = 0; i <= samples; ++i) {
        double d = total_dist * i / samples;
        p.distances_nm.push_back(d);
        // find segment
        size_t seg = 1;
        while (seg < wpts.size() && wpts[seg].distance_nm < d) ++seg;
        if (seg >= wpts.size()) {
            p.altitudes_ft.push_back(wpts.back().altitude_ft);
        } else {
            const auto& a = wpts[seg - 1];
            const auto& b = wpts[seg];
            double t = (d - a.distance_nm) / (b.distance_nm - a.distance_nm);
            double alt = a.altitude_ft + t * (b.altitude_ft - a.altitude_ft);
            p.altitudes_ft.push_back(alt);
        }
    }
    return p;
}

static void render_ascii(const ProfilePoints& p) {
    if (p.distances_nm.empty()) {
        std::cout << "No profile to render.\n";
        return;
    }
    double max_alt = *std::max_element(p.altitudes_ft.begin(), p.altitudes_ft.end());
    double min_alt = *std::min_element(p.altitudes_ft.begin(), p.altitudes_ft.end());
    if (max_alt == min_alt) max_alt += 100.0;
    const int rows = 20;
    int cols = static_cast<int>(p.distances_nm.size());
    std::vector<std::string> grid(rows, std::string(cols, ' '));
    for (int i = 0; i < cols; ++i) {
        double alt = p.altitudes_ft[i];
        int r = static_cast<int>(std::round((alt - min_alt) / (max_alt - min_alt) * (rows - 1)));
        r = std::clamp(r, 0, rows - 1);
        grid[rows - 1 - r][i] = '*';
    }
    for (int r = 0; r < rows; ++r) {
        double alt_mark = min_alt + (max_alt - min_alt) * (rows - 1 - r) / (rows - 1);
        std::cout << std::setw(6) << static_cast<int>(std::round(alt_mark)) << " | ";
        std::cout << grid[r] << "\n";
    }
    std::cout << "       ";
    for (int i = 0; i < cols; i += 10) {
        std::cout << std::setw(10) << static_cast<int>(std::round(p.distances_nm[i]));
    }
    std::cout << " nm\n";
}

static double find_distance_to_alt(const std::vector<Waypoint>& wpts, double start_alt,
                                   double target_alt, double gradient_ft_per_nm) {
    if (gradient_ft_per_nm <= 0) return 0.0;
    double delta_ft = target_alt - start_alt;
    return delta_ft / gradient_ft_per_nm;
}

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --route route.csv [--climb 300] [--descent 250] [--samples 200]\n";
    std::cerr << " route.csv columns: name,distance_nm,altitude_ft (cumulative distance)\n";
}

int main(int argc, char** argv) {
    std::string route_path;
    double climb_grad = 300.0;   // ft per nm
    double descent_grad = 250.0; // ft per nm
    int samples = 200;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--route" && i + 1 < argc) {
            route_path = argv[++i];
        } else if (arg == "--climb" && i + 1 < argc) {
            climb_grad = std::stod(argv[++i]);
        } else if (arg == "--descent" && i + 1 < argc) {
            descent_grad = std::stod(argv[++i]);
        } else if (arg == "--samples" && i + 1 < argc) {
            samples = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (route_path.empty()) {
        usage(argv[0]);
        return 1;
    }
    auto route = load_route(route_path);
    if (route.size() < 2) {
        std::cerr << "Route needs at least 2 waypoints.\n";
        return 1;
    }
    double total_dist = route.back().distance_nm;
    double dep_alt = route.front().altitude_ft;
    double dest_alt = route.back().altitude_ft;
    double cruise_alt = dep_alt;
    for (const auto& w : route) cruise_alt = std::max(cruise_alt, w.altitude_ft);

    double dist_to_toc = find_distance_to_alt(route, dep_alt, cruise_alt, climb_grad);
    double dist_from_dest_tod = find_distance_to_alt(route, dest_alt, cruise_alt, descent_grad);
    double tod_at = total_dist - dist_from_dest_tod;
    if (tod_at < 0) tod_at = 0;

    std::cout << "Total distance: " << total_dist << " nm\n";
    std::cout << "Cruise altitude: " << cruise_alt << " ft\n";
    std::cout << "TOC ~ " << dist_to_toc << " nm from departure\n";
    std::cout << "TOD ~ " << dist_from_dest_tod << " nm from destination (at " << tod_at
              << " nm along route)\n\n";

    auto profile = interpolate_profile(route, samples);
    render_ascii(profile);
    return 0;
}
