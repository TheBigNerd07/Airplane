// NOTAM parser/scorer: fetches (via curl) or loads NOTAMs from a file, flags key hazards, and scores risk.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

struct Notam {
    std::string raw;
    std::string icao;
    bool runway_closure = false;
    bool approach_change = false;
    bool gps_outage = false;
    bool lighting_issue = false;
};

struct RiskScore {
    int score = 0;
    std::vector<std::string> reasons;
};

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::istringstream iss(text);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(iss, line)) {
        line = trim(line);
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

static std::optional<std::string> fetch_notams_http(const std::string& icao) {
    // Source: FAA/D-NOTAM (example static feed). For offline use, prefer --file.
    std::string url = "https://www.notams.faa.gov/dinsQueryWeb/queryRetrievalMapAction.do?retrieveLocId="
                      + icao + "&actionType=notamRetrievalByICAOs";
    std::string cmd = "curl -s --max-time 6 \"" + url + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return std::nullopt;
    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    if (output.empty()) return std::nullopt;
    return output;
}

static std::vector<Notam> parse_notams_text(const std::string& text, const std::string& icao_hint) {
    auto lines = split_lines(text);
    std::vector<Notam> out;
    std::regex icao_re(R"(([A-Z]{4}))");
    for (const auto& line : lines) {
        Notam n;
        n.raw = line;
        n.icao = icao_hint;
        std::smatch m;
        if (std::regex_search(line, m, icao_re)) {
            n.icao = m[1];
        }
        std::string up = line;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        if (up.find("RWY") != std::string::npos && (up.find("CLSD") != std::string::npos || up.find("CLOSED") != std::string::npos)) {
            n.runway_closure = true;
        }
        if (up.find("ILS") != std::string::npos || up.find("RNAV") != std::string::npos ||
            up.find("APCH") != std::string::npos || up.find("APPROACH") != std::string::npos) {
            if (up.find("U/S") != std::string::npos || up.find("UNUSABLE") != std::string::npos ||
                up.find("OUT OF SERVICE") != std::string::npos || up.find("NOT AVBL") != std::string::npos) {
                n.approach_change = true;
            }
        }
        if (up.find("GPS") != std::string::npos && (up.find("UNREL") != std::string::npos || up.find("OUTAGE") != std::string::npos || up.find("JAMMING") != std::string::npos)) {
            n.gps_outage = true;
        }
        if (up.find("RCLL") != std::string::npos || up.find("RWY LGTS") != std::string::npos ||
            up.find("PAPI") != std::string::npos || up.find("VASI") != std::string::npos ||
            up.find("MALSR") != std::string::npos || up.find("MIRL") != std::string::npos ||
            up.find("HIRL") != std::string::npos) {
            if (up.find("U/S") != std::string::npos || up.find("UNSERVICEABLE") != std::string::npos ||
                up.find("OUT OF SERVICE") != std::string::npos || up.find("OUTAGE") != std::string::npos ||
                up.find("NOT AVBL") != std::string::npos) {
                n.lighting_issue = true;
            }
        }
        out.push_back(n);
    }
    return out;
}

static RiskScore score_notams(const std::vector<Notam>& ns, const std::string& icao) {
    RiskScore r;
    r.score = 0;
    auto add = [&](int pts, const std::string& why) {
        r.score += pts;
        r.reasons.push_back(why);
    };
    for (const auto& n : ns) {
        if (!icao.empty() && !n.icao.empty() && n.icao != icao) continue;
        if (n.runway_closure) add(4, "Runway closure");
        if (n.approach_change) add(3, "Approach/NAVAID out");
        if (n.gps_outage) add(2, "GPS unreliability");
        if (n.lighting_issue) add(1, "Runway/approach lighting issue");
    }
    return r;
}

static void print_notams(const std::vector<Notam>& ns) {
    for (size_t i = 0; i < ns.size(); ++i) {
        const auto& n = ns[i];
        std::cout << "[" << (i + 1) << "] " << n.raw << "\n";
        std::cout << "     Flags: ";
        bool any = false;
        if (n.runway_closure) { std::cout << "runway-closure "; any = true; }
        if (n.approach_change) { std::cout << "approach-out "; any = true; }
        if (n.gps_outage) { std::cout << "gps-outage "; any = true; }
        if (n.lighting_issue) { std::cout << "lighting-issue "; any = true; }
        if (!any) std::cout << "none";
        std::cout << "\n";
    }
}

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " --icao KJFK [--file notams.txt] [--risk-only]\n";
    std::cerr << "  --icao     ICAO code to analyze\n";
    std::cerr << "  --file     Path to local NOTAM text (if omitted, will try live fetch via curl)\n";
    std::cerr << "  --risk-only  Only print risk score\n";
}

int main(int argc, char** argv) {
    std::string icao;
    std::string file_path;
    bool risk_only = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--icao" && i + 1 < argc) {
            icao = argv[++i];
            std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
        } else if (arg == "--file" && i + 1 < argc) {
            file_path = argv[++i];
        } else if (arg == "--risk-only") {
            risk_only = true;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (icao.empty()) {
        usage(argv[0]);
        return 1;
    }

    std::string raw_text;
    if (!file_path.empty()) {
        std::ifstream f(file_path);
        if (!f.is_open()) {
            std::cerr << "Could not open NOTAM file: " << file_path << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        raw_text = buffer.str();
    } else {
        auto fetched = fetch_notams_http(icao);
        if (!fetched) {
            std::cerr << "Failed to fetch NOTAMs (offline?). Provide --file <path> to a saved NOTAM list.\n";
            return 1;
        }
        raw_text = *fetched;
    }

    auto parsed = parse_notams_text(raw_text, icao);
    auto risk = score_notams(parsed, icao);

    if (!risk_only) {
        std::cout << "NOTAMs for " << icao << " (" << parsed.size() << "):\n";
        print_notams(parsed);
        std::cout << "\n";
    }
    std::cout << "Risk score for " << icao << ": " << risk.score;
    if (!risk.reasons.empty()) {
        std::cout << " (";
        for (size_t i = 0; i < risk.reasons.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << risk.reasons[i];
        }
        std::cout << ")";
    }
    std::cout << "\n";
    return 0;
}
