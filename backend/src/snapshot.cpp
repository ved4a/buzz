#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include <cpu.hpp>
#include <memory.hpp>
#include <processes.hpp>
#include <disk.hpp>
#include <network.hpp>
#include <battery.hpp>
#include <snapshot.hpp>

using json = nlohmann::json;

namespace snapshot
{
    static std::string current_timestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        gmtime_r(&t, &tm);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    }

    json make()
    {
        json j;

        // CPU
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
            j["cpu"] = std::move(cpu_json);
        }

        // Memory
        {
            json mem_json;
            mem_json["memory_usage"] = get_memory_usage();
            mem_json["cached_memory"] = get_mem_value("Cached:");
            mem_json["free_swappable_memory"] = get_mem_value("SwapFree:");
            mem_json["total_swappable_memory"] = get_mem_value("SwapTotal:");
            j["memory"] = std::move(mem_json);
        }

        // Processes
        {
            json proc_json;
            auto processes = get_all_processes();
            for (const auto &p : processes)
                proc_json["processes"].push_back(process_to_json(p));
            j["process_info"] = std::move(proc_json);
        }

        // Disk
        {
            json disk_json;
            auto disks = get_disk_stats();
            for (const auto &d : disks)
                disk_json["disks"].push_back(disk_to_json(d));
            j["disk"] = std::move(disk_json);
        }

        // Battery
        j["battery"] = battery_to_json(get_battery_info());

        // Network
        {
            json net_json;
            auto net_stats = get_network_rates();
            for (const auto &iface : net_stats)
                net_json["interfaces"].push_back(network_to_json(iface));
            j["network"] = std::move(net_json);
        }

        j["timestamp"] = current_timestamp();
        return j;
    }

    std::string default_filename()
    {
        // buzz-snapshot-YYYYMMDD-HHMMSSZ.json
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        gmtime_r(&t, &tm);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%SZ", &tm);
        std::ostringstream oss;
        oss << "buzz-snapshot-" << buf << ".json";
        return oss.str();
    }

    bool save_to_file(const json &j, const std::string &path, std::string *err)
    {
        std::ofstream ofs(path);
        if (!ofs.is_open())
        {
            if (err)
                *err = "Failed to open file for writing: " + path;
            return false;
        }
        ofs << j.dump(4);
        if (!ofs.good())
        {
            if (err)
                *err = "Failed writing snapshot to: " + path;
            return false;
        }
        return true;
    }
}
