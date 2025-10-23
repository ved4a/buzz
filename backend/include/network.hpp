#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct NetworkStats
{
    std::string interface;
    double upload_rate;
    double download_rate;
};

std::vector<NetworkStats> get_network_rates();
nlohmann::json network_to_json(const NetworkStats &iface);

#endif