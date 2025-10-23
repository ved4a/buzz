#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include <cpu.hpp>
#include <memory.hpp>
#include <processes.hpp>
#include <disk.hpp>
#include <network.hpp>
#include <battery.hpp>
#include "cli.hpp"

using json = nlohmann::json;

json collect_cpu_info()
{
    json cpu_json;

    double cpu_usage = get_cpu_usage();
    std::vector<double> per_core = get_per_core_usage();

    cpu_json["cpu_usage"] = cpu_usage;
    cpu_json["cpu_name"] = get_cpu_name();
    cpu_json["running_processes"] = get_running_processes();
    cpu_json["cpu_frequency"] = get_cpu_frequency();
    cpu_json["no_of_logical_processors"] = get_no_logical_processors();

    cpu_json["per_core_usage"] = json::array();
    for (size_t i = 0; i < per_core.size(); ++i)
        cpu_json["per_core_usage"].push_back({{"core_id", static_cast<int>(i)},
                                              {"usage_percent", per_core[i]}});

    return cpu_json;
}

json collect_memory_info()
{
    json mem_json;

    mem_json["memory_usage"] = get_memory_usage();
    mem_json["cached_memory"] = get_mem_value("Cached:");
    mem_json["free_swappable_memory"] = get_mem_value("SwapFree:");
    mem_json["total_swappable_memory"] = get_mem_value("SwapTotal:");

    return mem_json;
}

json collect_process_info()
{
    json proc_json;
    auto processes = get_all_processes();

    for (const auto &p : processes)
        proc_json["processes"].push_back(process_to_json(p));

    return proc_json;
}

json collect_disk_info()
{
    json disk_json;
    auto disks = get_disk_stats();

    for (const auto &d : disks)
        disk_json["disks"].push_back(disk_to_json(d));

    return disk_json;
}

json collect_battery_info()
{
    json battery_json;
    auto b = get_battery_info();
    battery_json = battery_to_json(b);
    return battery_json;
}

json collect_network_info()
{
    json net_json;
    auto net_stats = get_network_rates();

    for (const auto &iface : net_stats)
        net_json["interfaces"].push_back(network_to_json(iface));

    return net_json;
}

int main(int argc, char **argv)
{
    if (auto rc = cli::run(argc, argv))
    {
        return *rc; // handled a subcommand (e.g., --kill), exit now
    }
    json j;

    j["cpu"] = collect_cpu_info();
    j["memory"] = collect_memory_info();
    j["process_info"] = collect_process_info(); // list of ProcessInfo objects
    j["disk"] = collect_disk_info();
    j["battery"] = collect_battery_info();
    j["network"] = collect_network_info();

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    j["timestamp"] = buf;

    std::cout << j.dump(4) << std::endl;

    return 0;
}