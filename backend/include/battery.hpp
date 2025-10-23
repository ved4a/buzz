#ifndef BATTERY_HPP
#define BATTERY_HPP

#include <string>
#include <nlohmann/json.hpp>

struct BatteryInfo
{
    std::string status;
    int current_charge;
};

BatteryInfo get_battery_info();
nlohmann::json battery_to_json(const BatteryInfo &b);

#endif
