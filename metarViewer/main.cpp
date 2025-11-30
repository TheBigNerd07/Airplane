#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

constexpr double kPi = 3.14159265358979323846;

struct WindInfo {
    std::optional<int> direction_deg; // std::nullopt for VRB
    int speed_kt = 0;
    std::optional<int> gust_kt;
};

struct Minima {
    double min_ceiling_ft = 1000.0;
    double min_visibility_sm = 3.0;
    double max_crosswind_kt = 15.0;
};

struct MetarDecoded {
    std::string station;
    std::string timestamp_z;
    WindInfo wind;
    std::optional<double> visibility_sm;
    std::optional<int> ceiling_ft;
    std::string ceiling_layer;
    std::vector<std::string> weather;
};

static std::vector<std::string> split_tokens(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
    return tokens;
}

static bool is_number(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

static std::optional<double> parse_fraction(const std::string& token) {
    auto slash_pos = token.find('/');
    if (slash_pos == std::string::npos) {
        return std::nullopt;
    }
    std::string num_str = token.substr(0, slash_pos);
    std::string den_str = token.substr(slash_pos + 1);
    if (!is_number(num_str) || !is_number(den_str)) {
        return std::nullopt;
    }
    double num = std::stod(num_str);
    double den = std::stod(den_str);
    if (den == 0) {
        return std::nullopt;
    }
    return num / den;
}

// Handles "10SM", "1/2SM", "1 1/2SM"
static std::optional<double> parse_visibility_sm(const std::vector<std::string>& tokens) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        auto sm_pos = tok.find("SM");
        if (sm_pos == std::string::npos) {
            continue;
        }
        std::string value_part = tok.substr(0, sm_pos);
        double total = 0.0;
        if (!value_part.empty()) {
            if (auto frac = parse_fraction(value_part)) {
                total += *frac;
            } else if (is_number(value_part)) {
                total += std::stod(value_part);
            }
        }
        // Support a leading whole number token (e.g., "1 1/2SM")
        if (i > 0 && is_number(tokens[i - 1])) {
            total += std::stod(tokens[i - 1]);
        }
        if (total > 0.0) {
            return total;
        }
    }
    return std::nullopt;
}

static WindInfo parse_wind(const std::vector<std::string>& tokens) {
    std::regex wind_re(R"((\d{3}|VRB)(\d{2,3})(G(\d{2,3}))?KT)");
    for (const auto& tok : tokens) {
        std::smatch m;
        if (std::regex_match(tok, m, wind_re)) {
            WindInfo w;
            if (m[1] != "VRB") {
                w.direction_deg = std::stoi(m[1]);
            }
            w.speed_kt = std::stoi(m[2]);
            if (m[4].matched) {
                w.gust_kt = std::stoi(m[4]);
            }
            return w;
        }
    }
    return {};
}

static std::optional<int> parse_ceiling_ft(const std::vector<std::string>& tokens, std::string& layer_out) {
    std::optional<int> ceiling_ft;
    for (const auto& tok : tokens) {
        if (tok.rfind("OVC", 0) == 0 || tok.rfind("BKN", 0) == 0 || tok.rfind("VV", 0) == 0) {
            std::string height_str = tok.substr(tok[1] == 'V' ? 2 : 3, 3);
            if (is_number(height_str)) {
                int height = std::stoi(height_str) * 100;
                if (!ceiling_ft || height < *ceiling_ft) {
                    ceiling_ft = height;
                    layer_out = tok.substr(0, 3);
                }
            }
        }
    }
    return ceiling_ft;
}

static std::vector<std::string> parse_weather(const std::vector<std::string>& tokens) {
    // Simple decoder; looks for common codes inside tokens.
    static const std::map<std::string, std::string> wx_map = {
        {"TS", "thunderstorm"}, {"RA", "rain"},  {"DZ", "drizzle"},
        {"SN", "snow"},         {"SG", "snow grains"}, {"PL", "ice pellets"},
        {"FG", "fog"},          {"BR", "mist"}, {"HZ", "haze"},
        {"FU", "smoke"},        {"SH", "showers"}
    };
    std::vector<std::string> found;
    for (const auto& tok : tokens) {
        for (const auto& [code, desc] : wx_map) {
            if (tok.find(code) != std::string::npos) {
                if (std::find(found.begin(), found.end(), desc) == found.end()) {
                    found.push_back(desc);
                }
            }
        }
    }
    return found;
}

static MetarDecoded decode_metar(const std::string& raw) {
    MetarDecoded m;
    auto tokens_raw = split_tokens(raw);
    std::vector<std::string> tokens;
    tokens.reserve(tokens_raw.size());
    for (auto t : tokens_raw) {
        std::transform(t.begin(), t.end(), t.begin(), ::toupper);
        tokens.push_back(std::move(t));
    }
    if (!tokens.empty()) {
        m.station = tokens[0];
    }
    if (tokens.size() > 1 && tokens[1].size() >= 5 && tokens[1].back() == 'Z') {
        m.timestamp_z = tokens[1];
    }
    m.wind = parse_wind(tokens);
    m.visibility_sm = parse_visibility_sm(tokens);
    m.ceiling_ft = parse_ceiling_ft(tokens, m.ceiling_layer);
    m.weather = parse_weather(tokens);
    return m;
}

static std::string format_double(double v, int precision = 1) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(precision);
    oss << v;
    return oss.str();
}

static void analyze_wind(const WindInfo& wind, int runway_heading_deg, const Minima& minima) {
    std::cout << "- Wind: ";
    if (!wind.direction_deg) {
        std::cout << "VRB " << wind.speed_kt << "kt";
        if (wind.gust_kt) std::cout << " G" << *wind.gust_kt;
        std::cout << " (variable direction)\n";
        return;
    }
    int dir = *wind.direction_deg;
    std::cout << dir << "@" << wind.speed_kt << "kt";
    if (wind.gust_kt) std::cout << " G" << *wind.gust_kt;
    if (runway_heading_deg == 0) {
        std::cout << " | add --runway <mag heading> for crosswind calc\n";
        return;
    }
    double angle_diff_rad = std::fabs(dir - runway_heading_deg) * kPi / 180.0;
    // Normalize to 0-180
    if (angle_diff_rad > kPi) angle_diff_rad = 2 * kPi - angle_diff_rad;
    double headwind = std::cos(angle_diff_rad) * wind.speed_kt;
    double crosswind = std::sin(angle_diff_rad) * wind.speed_kt;
    std::cout << " | headwind " << format_double(headwind) << " kt, crosswind "
              << format_double(std::fabs(crosswind)) << " kt";
    if (std::fabs(crosswind) > minima.max_crosswind_kt) {
        std::cout << " (EXCEEDS " << minima.max_crosswind_kt << " kt)\n";
    } else {
        std::cout << " (OK <= " << minima.max_crosswind_kt << " kt)\n";
    }
}

static void analyze_metar(const std::string& raw_metar, const Minima& minima, int runway_heading_deg) {
    auto m = decode_metar(raw_metar);
    std::cout << "Station: " << (m.station.empty() ? "N/A" : m.station);
    if (!m.timestamp_z.empty()) {
        std::cout << " @ " << m.timestamp_z;
    }
    std::cout << "\n";

    analyze_wind(m.wind, runway_heading_deg, minima);

    std::cout << "- Visibility: ";
    if (m.visibility_sm) {
        std::cout << format_double(*m.visibility_sm) << " SM";
        if (*m.visibility_sm < minima.min_visibility_sm) {
            std::cout << " (BELOW " << minima.min_visibility_sm << " SM)";
        } else {
            std::cout << " (OK >= " << minima.min_visibility_sm << " SM)";
        }
        std::cout << "\n";
    } else {
        std::cout << "N/A\n";
    }

    std::cout << "- Ceiling: ";
    if (m.ceiling_ft) {
        std::cout << *m.ceiling_ft << " ft " << m.ceiling_layer;
        if (*m.ceiling_ft < minima.min_ceiling_ft) {
            std::cout << " (BELOW " << minima.min_ceiling_ft << " ft)";
        } else {
            std::cout << " (OK >= " << minima.min_ceiling_ft << " ft)";
        }
        std::cout << "\n";
    } else {
        std::cout << "No ceiling reported\n";
    }

    std::cout << "- Weather: ";
    if (!m.weather.empty()) {
        for (size_t i = 0; i < m.weather.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << m.weather[i];
        }
        std::cout << "\n";
    } else {
        std::cout << "None significant\n";
    }
}

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --metar \"RAW METAR\" [--taf \"RAW TAF\"] [--runway 220] "
                 "[--min-ceiling 1000] [--min-vis 3] [--max-xwind 15]\n";
}

int main(int argc, char** argv) {
    std::string metar_raw;
    std::string taf_raw;
    Minima minima;
    int runway_heading = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--metar" || arg == "-m") && i + 1 < argc) {
            metar_raw = argv[++i];
        } else if ((arg == "--taf" || arg == "-t") && i + 1 < argc) {
            taf_raw = argv[++i];
        } else if (arg == "--runway" && i + 1 < argc) {
            runway_heading = std::stoi(argv[++i]);
        } else if (arg == "--min-ceiling" && i + 1 < argc) {
            minima.min_ceiling_ft = std::stod(argv[++i]);
        } else if (arg == "--min-vis" && i + 1 < argc) {
            minima.min_visibility_sm = std::stod(argv[++i]);
        } else if (arg == "--max-xwind" && i + 1 < argc) {
            minima.max_crosswind_kt = std::stod(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (metar_raw.empty()) {
        usage(argv[0]);
        return 1;
    }

    std::cout << "=== METAR ===\n" << metar_raw << "\n";
    if (runway_heading == 0) {
        std::cout << "(Tip: add --runway <mag heading> to compute crosswind)\n";
        runway_heading = 0;
    }
    analyze_metar(metar_raw, minima, runway_heading == 0 ? 0 : runway_heading);

    if (!taf_raw.empty()) {
        std::cout << "\n=== TAF (raw) ===\n" << taf_raw << "\n";
    }
    return 0;
}
