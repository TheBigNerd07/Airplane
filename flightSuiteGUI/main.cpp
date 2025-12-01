// Flight Suite Launcher (text UI): wraps existing tools into a simple menu-driven interface.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string run_cmd(const std::string& cmd) {
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Failed to run command.\n";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    return output;
}

static bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static void metar_menu() {
    std::string metar, icao, runway, hist;
    std::cout << "Enter raw METAR (leave blank to fetch via --icao): ";
    std::getline(std::cin, metar);
    std::cout << "Enter ICAO to fetch (optional): ";
    std::getline(std::cin, icao);
    std::cout << "How many METARs to fetch (history, optional): ";
    std::getline(std::cin, hist);
    std::cout << "Runway heading (deg, optional): ";
    std::getline(std::cin, runway);
    std::stringstream cmd;
    cmd << "../metarViewer/wx_brief ";
    if (!metar.empty()) cmd << "--metar \"" << metar << "\" ";
    if (!icao.empty()) cmd << "--icao " << icao << " ";
    if (!hist.empty()) cmd << "--icao-history " << hist << " ";
    if (!runway.empty()) cmd << "--runway " << runway << " ";
    auto out = run_cmd(cmd.str());
    std::cout << out << "\n";
}

static void flight_ideas_menu() {
    std::string count, region, random_start;
    std::cout << "Suggestions per aircraft [3]: ";
    std::getline(std::cin, count);
    std::cout << "Region filter (e.g., US-WA) [blank for any]: ";
    std::getline(std::cin, region);
    std::cout << "Random departures? (y/n) [n]: ";
    std::getline(std::cin, random_start);
    std::stringstream cmd;
    cmd << "../flightIdeas/route_suggester ";
    if (!count.empty()) cmd << "--count " << count << " ";
    if (!region.empty()) cmd << "--region " << region << " ";
    if (!random_start.empty() && (random_start[0] == 'y' || random_start[0] == 'Y')) {
        cmd << "--random-start ";
    }
    auto out = run_cmd(cmd.str());
    std::cout << out << "\n";
}

static void notam_menu() {
    std::string icao, file, risk_only;
    std::cout << "ICAO: ";
    std::getline(std::cin, icao);
    std::cout << "NOTAM file path (optional, uses curl otherwise): ";
    std::getline(std::cin, file);
    std::cout << "Risk only? (y/n) [n]: ";
    std::getline(std::cin, risk_only);
    std::stringstream cmd;
    cmd << "../notamTool/notam_risk --icao " << icao << " ";
    if (!file.empty()) cmd << "--file \"" << file << "\" ";
    if (!risk_only.empty() && (risk_only[0] == 'y' || risk_only[0] == 'Y')) cmd << "--risk-only ";
    auto out = run_cmd(cmd.str());
    std::cout << out << "\n";
}

static void e6b_menu() {
    std::cout << "Modes: winds, xwind, headwind, pressure_alt, density_alt, mach, tas, tsd, fuel, drift, groundspeed\n";
    std::cout << "Enter mode: ";
    std::string mode;
    std::getline(std::cin, mode);
    std::cout << "Enter args separated by space (per README): ";
    std::string args;
    std::getline(std::cin, args);
    std::stringstream cmd;
    cmd << "../e6bTool/e6b " << mode << " " << args;
    auto out = run_cmd(cmd.str());
    std::cout << out << "\n";
}

static void vertical_profile_menu() {
    std::string route, climb, descent, samples;
    std::cout << "Route CSV path [../verticalProfile/route_sample.csv]: ";
    std::getline(std::cin, route);
    std::cout << "Climb gradient ft/nm [300]: ";
    std::getline(std::cin, climb);
    std::cout << "Descent gradient ft/nm [250]: ";
    std::getline(std::cin, descent);
    std::cout << "Samples [200]: ";
    std::getline(std::cin, samples);
    std::stringstream cmd;
    cmd << "../verticalProfile/vert_profile ";
    cmd << "--route " << (route.empty() ? "../verticalProfile/route_sample.csv" : route) << " ";
    if (!climb.empty()) cmd << "--climb " << climb << " ";
    if (!descent.empty()) cmd << "--descent " << descent << " ";
    if (!samples.empty()) cmd << "--samples " << samples << " ";
    auto out = run_cmd(cmd.str());
    std::cout << out << "\n";
}

static void simbrief_menu() {
    std::string ofp, out;
    std::cout << "SimBrief OFP XML path [./ofp.xml]: ";
    std::getline(std::cin, ofp);
    std::cout << "Output CSV path [../verticalProfile/route_sample.csv]: ";
    std::getline(std::cin, out);
    std::stringstream cmd;
    std::string ofp_path = ofp.empty() ? "./ofp.xml" : ofp;
    cmd << "../simbriefBrief/simbrief_brief --ofp \"" << ofp_path << "\" ";
    if (!out.empty()) {
        cmd << "--csv \"" << out << "\"";
    } else {
        cmd << "--csv \"../verticalProfile/route_sample.csv\"";
    }
    auto res = run_cmd(cmd.str());
    std::cout << res << "\n";
}

static void menu() {
    while (true) {
        std::cout << "\nFlight Suite Launcher\n";
        std::cout << "1) METAR Decoder\n";
        std::cout << "2) Route Suggester\n";
        std::cout << "3) NOTAM Risk\n";
        std::cout << "4) E6B Calculator\n";
        std::cout << "5) Vertical Profile\n";
        std::cout << "6) SimBrief Summary / Route -> CSV\n";
        std::cout << "7) Quit\n";
        std::cout << "Select: ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "1") {
            if (file_exists("../metarViewer/wx_brief")) metar_menu();
            else std::cout << "Build ../metarViewer/wx_brief first.\n";
        } else if (choice == "2") {
            if (file_exists("../flightIdeas/route_suggester")) flight_ideas_menu();
            else std::cout << "Build ../flightIdeas/route_suggester first.\n";
        } else if (choice == "3") {
            if (file_exists("../notamTool/notam_risk")) notam_menu();
            else std::cout << "Build ../notamTool/notam_risk first.\n";
        } else if (choice == "4") {
            if (file_exists("../e6bTool/e6b")) e6b_menu();
            else std::cout << "Build ../e6bTool/e6b first.\n";
        } else if (choice == "5") {
            if (file_exists("../verticalProfile/vert_profile")) vertical_profile_menu();
            else std::cout << "Build ../verticalProfile/vert_profile first.\n";
        } else if (choice == "6") {
            if (file_exists("../simbriefBrief/simbrief_brief")) simbrief_menu();
            else std::cout << "Build ../simbriefBrief/simbrief_brief first.\n";
        } else if (choice == "7" || choice == "q" || choice == "Q") {
            break;
        } else {
            std::cout << "Invalid choice.\n";
        }
    }
}

int main() {
    menu();
    return 0;
}
