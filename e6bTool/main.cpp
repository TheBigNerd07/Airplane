// E6B flight computer CLI: provides common flight calculations.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

constexpr double kPi = 3.14159265358979323846;
constexpr double kRho0 = 1.225; // kg/m^3 sea-level

static double deg2rad(double d) { return d * kPi / 180.0; }
static double rad2deg(double r) { return r * 180.0 / kPi; }

static void print_result(const std::string& label, double value, const std::string& unit = "") {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << label << ": " << value;
    if (!unit.empty()) std::cout << " " << unit;
    std::cout << "\n";
}

// Basic wind triangle: returns ground speed and track using heading, true airspeed, wind direction/speed.
static void wind_triangle(double hdg_deg, double tas_kt, double wind_dir_deg, double wind_spd_kt,
                          double& out_gs, double& out_track_deg, double& out_wca_deg) {
    double hdg = deg2rad(hdg_deg);
    double wind_dir = deg2rad(wind_dir_deg);
    double wx = wind_spd_kt * std::sin(wind_dir);
    double wy = wind_spd_kt * std::cos(wind_dir);
    double tx = tas_kt * std::sin(hdg) + wx;
    double ty = tas_kt * std::cos(hdg) + wy;
    out_track_deg = rad2deg(std::atan2(tx, ty));
    if (out_track_deg < 0) out_track_deg += 360.0;
    out_gs = std::sqrt(tx * tx + ty * ty);
    double desired_track_rad = deg2rad(out_track_deg);
    double wca = std::asin((wind_spd_kt * std::sin(wind_dir - desired_track_rad)) / tas_kt);
    out_wca_deg = rad2deg(wca);
}

static double crosswind_component(double wind_dir_deg, double wind_spd_kt, double runway_deg) {
    double angle = std::fabs(wind_dir_deg - runway_deg);
    if (angle > 180.0) angle = 360.0 - angle;
    return wind_spd_kt * std::sin(deg2rad(angle));
}

static double headwind_component(double wind_dir_deg, double wind_spd_kt, double runway_deg) {
    double angle = std::fabs(wind_dir_deg - runway_deg);
    if (angle > 180.0) angle = 360.0 - angle;
    return wind_spd_kt * std::cos(deg2rad(angle));
}

static double pressure_altitude_ft(double field_elev_ft, double altimeter_inhg) {
    return field_elev_ft + (29.92 - altimeter_inhg) * 1000.0;
}

static double density_altitude_ft(double pressure_alt_ft, double oat_c) {
    // Simple approximation: DA = PA + 120 * (OAT - ISA)
    double isa_temp_c = 15.0 - (pressure_alt_ft / 1000.0) * 2.0;
    return pressure_alt_ft + 120.0 * (oat_c - isa_temp_c);
}

static double mach_from_tas(double tas_kt, double oat_c) {
    // a = sqrt(gamma*R*T), gamma=1.4, R=287 J/kg/K; TAS in kt -> m/s
    double tas_ms = tas_kt * 0.514444;
    double temp_k = oat_c + 273.15;
    double a_ms = std::sqrt(1.4 * 287.0 * temp_k);
    return tas_ms / a_ms;
}

static double tas_from_mach(double mach, double oat_c) {
    double temp_k = oat_c + 273.15;
    double a_ms = std::sqrt(1.4 * 287.0 * temp_k);
    double tas_ms = mach * a_ms;
    return tas_ms / 0.514444;
}

static void usage(const char* prog) {
    std::cout << "E6B flight computer\n";
    std::cout << "Usage: " << prog << " <mode> [args]\n";
    std::cout << " Modes:\n";
    std::cout << "  winds        <hdg_deg> <tas_kt> <wind_dir_deg> <wind_spd_kt>\n";
    std::cout << "  xwind        <wind_dir_deg> <wind_spd_kt> <runway_deg>\n";
    std::cout << "  headwind     <wind_dir_deg> <wind_spd_kt> <runway_deg>\n";
    std::cout << "  pressure_alt <field_elev_ft> <altimeter_inhg>\n";
    std::cout << "  density_alt  <field_elev_ft> <altimeter_inhg> <oat_c>\n";
    std::cout << "  mach         <tas_kt> <oat_c>\n";
    std::cout << "  tas          <mach> <oat_c>\n";
    std::cout << "  tsd          <distance_nm> <groundspeed_kt>   (time in minutes)\n";
    std::cout << "  fuel         <flow_gph> <time_hr>\n";
    std::cout << "  drift        <wind_dir_deg> <wind_spd_kt> <tas_kt> <track_deg>\n";
    std::cout << "  groundspeed  <tas_kt> <wind_component_kt>\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    std::string mode = argv[1];
    if (mode == "winds" && argc == 6) {
        double hdg = std::stod(argv[2]);
        double tas = std::stod(argv[3]);
        double wdir = std::stod(argv[4]);
        double wspd = std::stod(argv[5]);
        double gs, track, wca;
        wind_triangle(hdg, tas, wdir, wspd, gs, track, wca);
        print_result("Groundspeed", gs, "kt");
        print_result("Resulting track", track, "deg");
        print_result("Wind correction angle", wca, "deg");
    } else if (mode == "xwind" && argc == 5) {
        double wdir = std::stod(argv[2]);
        double wspd = std::stod(argv[3]);
        double rwy = std::stod(argv[4]);
        print_result("Crosswind", crosswind_component(wdir, wspd, rwy), "kt");
    } else if (mode == "headwind" && argc == 5) {
        double wdir = std::stod(argv[2]);
        double wspd = std::stod(argv[3]);
        double rwy = std::stod(argv[4]);
        print_result("Headwind", headwind_component(wdir, wspd, rwy), "kt");
    } else if (mode == "pressure_alt" && argc == 4) {
        double elev = std::stod(argv[2]);
        double altim = std::stod(argv[3]);
        print_result("Pressure altitude", pressure_altitude_ft(elev, altim), "ft");
    } else if (mode == "density_alt" && argc == 5) {
        double elev = std::stod(argv[2]);
        double altim = std::stod(argv[3]);
        double oat = std::stod(argv[4]);
        double pa = pressure_altitude_ft(elev, altim);
        print_result("Pressure altitude", pa, "ft");
        print_result("Density altitude", density_altitude_ft(pa, oat), "ft");
    } else if (mode == "mach" && argc == 4) {
        double tas = std::stod(argv[2]);
        double oat = std::stod(argv[3]);
        print_result("Mach", mach_from_tas(tas, oat), "M");
    } else if (mode == "tas" && argc == 4) {
        double mach = std::stod(argv[2]);
        double oat = std::stod(argv[3]);
        print_result("TAS", tas_from_mach(mach, oat), "kt");
    } else if (mode == "tsd" && argc == 4) {
        double dist = std::stod(argv[2]);
        double gs = std::stod(argv[3]);
        print_result("Time", (dist / gs) * 60.0, "min");
    } else if (mode == "fuel" && argc == 4) {
        double flow = std::stod(argv[2]);
        double time_hr = std::stod(argv[3]);
        print_result("Fuel used", flow * time_hr, "gal");
    } else if (mode == "drift" && argc == 6) {
        double wdir = std::stod(argv[2]);
        double wspd = std::stod(argv[3]);
        double tas = std::stod(argv[4]);
        double track = std::stod(argv[5]);
        double wca = rad2deg(std::asin((wspd / tas) * std::sin(deg2rad(wdir - track))));
        print_result("Drift angle", wca, "deg");
    } else if (mode == "groundspeed" && argc == 4) {
        double tas = std::stod(argv[2]);
        double wind_comp = std::stod(argv[3]);
        print_result("Groundspeed", tas + wind_comp, "kt");
    } else {
        usage(argv[0]);
        return 1;
    }
    return 0;
}
