#include "battery.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

BatteryInfo get_battery_info()
{
    BatteryInfo b;
    std::string battery_path = "/sys/class/power_supply/";

    // try BAT0 first, then BAT1 if BAT0 is unavailable
    for (const std::string &battery : {"BAT0", "BAT1"})
    {
        std::ifstream status_file(battery_path + battery + "/status");
        std::ifstream battery_percentage_file(battery_path + battery + "/capacity");

        if (status_file && battery_percentage_file)
        {
            std::getline(status_file, b.status);

            if (!(battery_percentage_file >> b.current_charge))
            {
                b.current_charge = -1;
            }
            break;
        }
        else
        {
            b.status = "Unavailable";
            b.current_charge = -1;
        }
    }

    return b;
}

json battery_to_json(const BatteryInfo &b)
{
    return {
        {"status", b.status},
        {"current_capacity", b.current_charge},
    };
}
