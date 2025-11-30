// Simple flight log updater: prompts for flight details and appends to a CSV log file.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct FlightEntry {
    std::string date;       // YYYY-MM-DD
    std::string tail;       // aircraft tail/registration
    std::string from;       // departure ICAO
    std::string to;         // arrival ICAO
    std::string route;      // optional route string
    double pic_hours = 0.0; // PIC time
    double sic_hours = 0.0; // SIC time
    double night_hours = 0.0;
    double ifr_hours = 0.0;
    int landings_day = 0;
    int landings_night = 0;
    std::string remarks;
};

static std::string prompt(const std::string& label, const std::string& def = "") {
    std::cout << label;
    if (!def.empty()) std::cout << " [" << def << "]";
    std::cout << ": ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty()) return def;
    return input;
}

static double prompt_double(const std::string& label, double def = 0.0) {
    while (true) {
        std::string in = prompt(label, def == 0.0 ? "" : std::to_string(def));
        if (in.empty()) return def;
        std::stringstream ss(in);
        double v = 0.0;
        if (ss >> v) return v;
        std::cout << "Please enter a number.\n";
    }
}

static int prompt_int(const std::string& label, int def = 0) {
    while (true) {
        std::string in = prompt(label, def == 0 ? "" : std::to_string(def));
        if (in.empty()) return def;
        std::stringstream ss(in);
        int v = 0;
        if (ss >> v) return v;
        std::cout << "Please enter an integer.\n";
    }
}

static FlightEntry collect_entry() {
    FlightEntry e;
    std::cout << "Enter flight details (leave blank to use defaults if shown)\n";
    e.date = prompt("Date (YYYY-MM-DD)");
    e.tail = prompt("Aircraft tail/registration");
    e.from = prompt("Departure ICAO");
    e.to = prompt("Arrival ICAO");
    e.route = prompt("Route (optional)");
    e.pic_hours = prompt_double("PIC hours", 0.0);
    e.sic_hours = prompt_double("SIC hours", 0.0);
    e.night_hours = prompt_double("Night hours", 0.0);
    e.ifr_hours = prompt_double("IFR/IMC hours", 0.0);
    e.landings_day = prompt_int("Day landings", 0);
    e.landings_night = prompt_int("Night landings", 0);
    e.remarks = prompt("Remarks (optional)");
    return e;
}

static void append_entry(const FlightEntry& e, const std::string& path) {
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open log file: " << path << "\n";
        return;
    }
    file << e.date << "," << e.tail << "," << e.from << "," << e.to << ","
         << e.route << "," << e.pic_hours << "," << e.sic_hours << "," << e.night_hours << ","
         << e.ifr_hours << "," << e.landings_day << "," << e.landings_night << "," << e.remarks
         << "\n";
    std::cout << "Saved to " << path << "\n";
}

int main(int argc, char** argv) {
    std::string log_path = "flight_log.csv";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--log" || arg == "-l") && i + 1 < argc) {
            log_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [--log path/to/log.csv]\n";
            return 0;
        } else {
            std::cout << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    // If file is empty/new, write header.
    std::ifstream check(log_path);
    bool need_header = !check.good() || check.peek() == std::ifstream::traits_type::eof();
    check.close();
    if (need_header) {
        std::ofstream init(log_path, std::ios::app);
        init << "date,tail,from,to,route,pic_hours,sic_hours,night_hours,ifr_hours,landings_day,landings_night,remarks\n";
    }

    FlightEntry e = collect_entry();
    append_entry(e, log_path);
    return 0;
}
