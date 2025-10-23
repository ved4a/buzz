#include "network.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <unordered_map>

using json = nlohmann::json;

struct RawNet
{
    std::string iface;
    long rx_bytes;
    long tx_bytes;
};

static std::vector<RawNet> read_raw_net()
{
    std::vector<RawNet> interfaces;
    std::ifstream file("/proc/net/dev");
    std::string line;

    // 2 skip the headers
    std::getline(file, line);
    std::getline(file, line);

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        RawNet r;
        std::getline(iss, r.iface, ':');
        iss >> r.rx_bytes;
        for (int i = 0; i < 7; ++i)
            iss >> line; // skippin info
        iss >> r.tx_bytes;

        // formatting
        r.iface.erase(0, r.iface.find_first_not_of(" "));
        interfaces.push_back(r);
    }
    return interfaces;
};

std::vector<NetworkStats> get_network_rates()
{
    auto before = read_raw_net();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto after = read_raw_net();

    std::unordered_map<std::string, RawNet> byName;
    byName.reserve(before.size());
    for (const auto &r : before)
        byName[r.iface] = r;

    std::vector<NetworkStats> results;
    results.reserve(after.size());

    for (const auto &r : after)
    {
        auto it = byName.find(r.iface);
        if (it == byName.end())
            continue;

        long drx = r.rx_bytes - it->second.rx_bytes;
        long dtx = r.tx_bytes - it->second.tx_bytes;
        if (drx < 0)
            drx = 0;
        if (dtx < 0)
            dtx = 0;

        NetworkStats iface;
        iface.interface = r.iface;
        iface.download_rate = static_cast<double>(drx);
        iface.upload_rate = static_cast<double>(dtx);
        results.push_back(iface);
    }
    return results;
};

json network_to_json(const NetworkStats &iface)
{
    return {
        {"interface", iface.interface},
        {"upload_rate_bytes_per_sec", iface.upload_rate},
        {"download_rate_bytes_per_sec", iface.download_rate}};
};