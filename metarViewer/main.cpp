#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

constexpr double kPi = 3.14159265358979323846;

enum class OutputFormat { Text, Json };

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

struct WindComponents {
    double headwind = 0.0;
    double crosswind = 0.0;
};

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

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

static std::string to_upper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::toupper);
    return out;
}

static std::optional<std::string> fetch_url(const std::string& url) {
    std::string cmd = "curl -s --max-time 5 \"" + url + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return std::nullopt;
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    if (output.empty()) return std::nullopt;
    return output;
}

static std::optional<std::string> fetch_metar_by_icao(const std::string& icao_raw) {
    if (icao_raw.size() < 3) return std::nullopt;
    std::string icao = to_upper(icao_raw);
    auto output = fetch_url("https://tgftp.nws.noaa.gov/data/observations/metar/stations/" + icao +
                            ".TXT");
    if (!output) return std::nullopt;

    std::istringstream iss(*output);
    std::string line;
    std::string last_non_empty;
    while (std::getline(iss, line)) {
        line = trim(line);
        if (!line.empty()) last_non_empty = line;
    }
    if (!last_non_empty.empty()) {
        return last_non_empty;
    }
    return std::nullopt;
}

static std::vector<std::string> fetch_cycle_metars_for_hour(const std::string& icao,
                                                            int hour_utc) {
    std::vector<std::string> results;
    char hour_buf[8];
    std::snprintf(hour_buf, sizeof(hour_buf), "%02d", hour_utc);
    std::string url =
        "https://tgftp.nws.noaa.gov/data/observations/metar/cycles/" + std::string(hour_buf) +
        "Z.TXT";
    auto content = fetch_url(url);
    if (!content) return results;
    std::istringstream iss(*content);
    std::string line;
    std::string target = to_upper(icao);
    while (std::getline(iss, line)) {
        line = trim(line);
        if (line.rfind(target + " ", 0) == 0) {
            results.push_back(line);
        }
    }
    return results;
}

static std::vector<std::string> fetch_metars_history(const std::string& icao, int desired_count) {
    std::vector<std::string> collected;
    std::time_t now = std::time(nullptr);
    const int max_hours = 48; // limit fetch window
    for (int back = 0; back < max_hours && (int)collected.size() < desired_count; ++back) {
        std::time_t t = now - back * 3600;
        std::tm* gmt = std::gmtime(&t);
        if (!gmt) break;
        int hour = gmt->tm_hour;
        auto hour_metars = fetch_cycle_metars_for_hour(icao, hour);
        for (const auto& m : hour_metars) {
            if ((int)collected.size() >= desired_count) break;
            // Avoid duplicates
            if (std::find(collected.begin(), collected.end(), m) == collected.end()) {
                collected.push_back(m);
            }
        }
    }
    // Data collected newest-first due to hour loop; reverse to oldest-first for trend logic
    std::reverse(collected.begin(), collected.end());
    return collected;
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
            int start = tok.rfind("VV", 0) == 0 ? 2 : 3;
            std::string height_str = tok.substr(start, 3);
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

static std::optional<WindComponents> compute_wind_components(const WindInfo& wind, int runway_heading_deg) {
    if (!wind.direction_deg || runway_heading_deg == 0) return std::nullopt;
    double angle_diff_rad = std::fabs(*wind.direction_deg - runway_heading_deg) * kPi / 180.0;
    if (angle_diff_rad > kPi) angle_diff_rad = 2 * kPi - angle_diff_rad;
    double headwind = std::cos(angle_diff_rad) * wind.speed_kt;
    double crosswind = std::sin(angle_diff_rad) * wind.speed_kt;
    return WindComponents{headwind, crosswind};
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
    auto comps = compute_wind_components(wind, runway_heading_deg);
    if (!comps) {
        std::cout << " | add --runway <mag heading> for crosswind calc\n";
        return;
    }
    double headwind = comps->headwind;
    double crosswind = comps->crosswind;
    std::cout << " | headwind " << format_double(headwind) << " kt, crosswind "
              << format_double(std::fabs(crosswind)) << " kt";
    if (std::fabs(crosswind) > minima.max_crosswind_kt) {
        std::cout << " (EXCEEDS " << minima.max_crosswind_kt << " kt)\n";
    } else {
        std::cout << " (OK <= " << minima.max_crosswind_kt << " kt)\n";
    }
}

static void analyze_metar(const std::string& raw_metar, const MetarDecoded& m, const Minima& minima,
                          int runway_heading_deg) {
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

static std::string trend_word(double delta) {
    const double tol = 0.05; // minimal change treated as flat
    if (delta > tol) return "improving";
    if (delta < -tol) return "worsening";
    return "steady";
}

static void print_trend_text(const std::vector<MetarDecoded>& mets) {
    if (mets.size() < 2) return;
    const MetarDecoded& first = mets.front();
    const MetarDecoded& last = mets.back();
    std::cout << "\n=== Trend (oldest -> latest) ===\n";
    if (first.visibility_sm && last.visibility_sm) {
        double delta = *last.visibility_sm - *first.visibility_sm;
        std::cout << "- Visibility: " << trend_word(delta) << " ("
                  << format_double(*first.visibility_sm) << " -> "
                  << format_double(*last.visibility_sm) << " SM)\n";
    }
    if (first.ceiling_ft && last.ceiling_ft) {
        double delta = static_cast<double>(*last.ceiling_ft - *first.ceiling_ft);
        std::cout << "- Ceiling: " << trend_word(delta) << " (" << *first.ceiling_ft << " -> "
                  << *last.ceiling_ft << " ft)\n";
    }
    if (first.wind.direction_deg && last.wind.direction_deg) {
        int delta_dir = *last.wind.direction_deg - *first.wind.direction_deg;
        std::cout << "- Wind: " << *first.wind.direction_deg << " -> "
                  << *last.wind.direction_deg << " deg";
        if (delta_dir != 0) std::cout << " (shift " << delta_dir << " deg)";
        std::cout << "\n";
    }
}

static void print_metar_json(const std::vector<std::string>& raws,
                             const std::vector<MetarDecoded>& mets, const Minima& minima,
                             int runway_heading_deg) {
    std::cout << "  \"metars\": [\n";
    for (size_t i = 0; i < mets.size(); ++i) {
        const auto& m = mets[i];
        std::cout << "    {\n";
        std::cout << "      \"raw\": \"" << raws[i] << "\",\n";
        std::cout << "      \"station\": \"" << m.station << "\",\n";
        std::cout << "      \"timestamp\": \"" << m.timestamp_z << "\",\n";
        std::cout << "      \"wind\": {";
        if (m.wind.direction_deg) {
            std::cout << "\"dir\":" << *m.wind.direction_deg << ",\"spd\":" << m.wind.speed_kt;
        } else {
            std::cout << "\"dir\":null,\"spd\":" << m.wind.speed_kt;
        }
        if (m.wind.gust_kt) {
            std::cout << ",\"gust\":" << *m.wind.gust_kt;
        }
        auto comps = compute_wind_components(m.wind, runway_heading_deg);
        if (comps) {
            std::cout << ",\"headwind\":" << format_double(comps->headwind)
                      << ",\"crosswind\":" << format_double(comps->crosswind);
        }
        std::cout << "},\n";
        std::cout << "      \"visibility_sm\": "
                  << (m.visibility_sm ? format_double(*m.visibility_sm) : "null") << ",\n";
        if (m.ceiling_ft) {
            std::cout << "      \"ceiling_ft\": " << *m.ceiling_ft << ",\n";
        } else {
            std::cout << "      \"ceiling_ft\": null,\n";
        }
        std::cout << "      \"ceiling_layer\": \"" << m.ceiling_layer << "\",\n";
        std::cout << "      \"weather\": [";
        for (size_t j = 0; j < m.weather.size(); ++j) {
            if (j) std::cout << ",";
            std::cout << "\"" << m.weather[j] << "\"";
        }
        std::cout << "],\n";
        std::cout << "      \"alerts\": {";
        bool first_alert = true;
        if (m.visibility_sm && *m.visibility_sm < minima.min_visibility_sm) {
            std::cout << "\"visibility\":\"below minima\"";
            first_alert = false;
        }
        if (m.ceiling_ft && *m.ceiling_ft < minima.min_ceiling_ft) {
            if (!first_alert) std::cout << ",";
            std::cout << "\"ceiling\":\"below minima\"";
            first_alert = false;
        }
        auto comps2 = compute_wind_components(m.wind, runway_heading_deg);
        if (comps2 && std::fabs(comps2->crosswind) > minima.max_crosswind_kt) {
            if (!first_alert) std::cout << ",";
            std::cout << "\"crosswind\":\"exceeds minima\"";
            first_alert = false;
        }
        std::cout << "}\n";
        std::cout << "    }" << (i + 1 == mets.size() ? "\n" : ",\n");
    }
    std::cout << "  ]";
}

static void print_trend_json(const std::vector<MetarDecoded>& mets) {
    std::cout << ",\n  \"trend\": ";
    if (mets.size() < 2) {
        std::cout << "null\n";
        return;
    }
    const auto& first = mets.front();
    const auto& last = mets.back();
    std::cout << "{";
    bool wrote_field = false;
    if (first.visibility_sm && last.visibility_sm) {
        std::cout << "\"visibility\":{\"from\":" << format_double(*first.visibility_sm)
                  << ",\"to\":" << format_double(*last.visibility_sm)
                  << ",\"state\":\"" << trend_word(*last.visibility_sm - *first.visibility_sm) << "\"}";
        wrote_field = true;
    }
    if (first.ceiling_ft && last.ceiling_ft) {
        if (wrote_field) std::cout << ",";
        std::cout << "\"ceiling\":{\"from\":" << *first.ceiling_ft << ",\"to\":" << *last.ceiling_ft
                  << ",\"state\":\"" << trend_word(*last.ceiling_ft - *first.ceiling_ft) << "\"}";
        wrote_field = true;
    }
    if (first.wind.direction_deg && last.wind.direction_deg) {
        if (wrote_field) std::cout << ",";
        std::cout << "\"wind_dir\":{\"from\":" << *first.wind.direction_deg
                  << ",\"to\":" << *last.wind.direction_deg << "}";
    }
    std::cout << "}\n";
}

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " (--metar \"RAW METAR\" ... | --icao KJFK [...]) [--icao-history N] [--taf \"RAW TAF\"] "
                 "[--runway 220] [--min-ceiling 1000] [--min-vis 3] [--max-xwind 15] "
                 "[--format text|json]\n";
}

int main(int argc, char** argv) {
    std::cout << " " << "\n";
    std::vector<std::string> metar_raws;
    std::vector<std::string> icaos;
    std::string taf_raw;
    Minima minima;
    int runway_heading = 0;
    OutputFormat format = OutputFormat::Text;
    int history_count = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--metar" || arg == "-m") && i + 1 < argc) {
            metar_raws.push_back(argv[++i]);
        } else if ((arg == "--taf" || arg == "-t") && i + 1 < argc) {
            taf_raw = argv[++i];
        } else if (arg == "--icao" && i + 1 < argc) {
            icaos.push_back(argv[++i]);
        } else if (arg == "--runway" && i + 1 < argc) {
            runway_heading = std::stoi(argv[++i]);
        } else if (arg == "--min-ceiling" && i + 1 < argc) {
            minima.min_ceiling_ft = std::stod(argv[++i]);
        } else if (arg == "--min-vis" && i + 1 < argc) {
            minima.min_visibility_sm = std::stod(argv[++i]);
        } else if (arg == "--max-xwind" && i + 1 < argc) {
            minima.max_crosswind_kt = std::stod(argv[++i]);
        } else if (arg == "--format" && i + 1 < argc) {
            std::string f = to_upper(argv[++i]);
            if (f == "JSON") {
                format = OutputFormat::Json;
            } else {
                format = OutputFormat::Text;
            }
        } else if (arg == "--icao-history" && i + 1 < argc) {
            history_count = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (metar_raws.empty() && icaos.empty()) {
        usage(argv[0]);
        return 1;
    }

    for (const auto& icao : icaos) {
        if (history_count > 0) {
            auto fetched = fetch_metars_history(icao, history_count);
            if (!fetched.empty()) {
                metar_raws.insert(metar_raws.end(), fetched.begin(), fetched.end());
            } else {
                std::cerr << "Failed to fetch historical METARs for " << icao << "\n";
            }
        } else {
            auto fetched = fetch_metar_by_icao(icao);
            if (fetched) {
                metar_raws.push_back(*fetched);
            } else {
                std::cerr << "Failed to fetch METAR for " << icao << "\n";
            }
        }
    }

    if (metar_raws.empty()) {
        std::cerr << "No METARs provided or fetched.\n";
        return 1;
    }

    std::vector<MetarDecoded> decoded;
    decoded.reserve(metar_raws.size());
    for (const auto& raw : metar_raws) {
        decoded.push_back(decode_metar(raw));
    }

    if (format == OutputFormat::Json) {
        std::cout << "{\n";
        print_metar_json(metar_raws, decoded, minima, runway_heading);
        print_trend_json(decoded);
        if (!taf_raw.empty()) {
            std::cout << ",\n  \"taf_raw\": \"" << taf_raw << "\"\n";
        } else {
            std::cout << "\n";
        }
        std::cout << "}\n";
        return 0;
    }

    for (size_t i = 0; i < metar_raws.size(); ++i) {
        std::cout << "=== METAR " << (i + 1) << " ===\n" << metar_raws[i] << "\n";
        if (runway_heading == 0) {
            std::cout << "(Tip: add --runway <mag heading> to compute crosswind)\n";
        }
        analyze_metar(metar_raws[i], decoded[i], minima, runway_heading == 0 ? 0 : runway_heading);
        if (i + 1 != metar_raws.size()) std::cout << "\n";
    }
    if (!taf_raw.empty()) {
        std::cout << "\n=== TAF (raw) ===\n" << taf_raw << "\n";
    }
    print_trend_text(decoded);
    return 0;
}
