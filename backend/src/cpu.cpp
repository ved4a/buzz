#include "cpu.hpp"
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>

// this is just for displaying CPU stats:
// 1. usage, 2. frequency (speed), 3. no of processes and threads

double get_cpu_usage();
std::string get_cpu_name();
long long get_running_processes();
double get_cpu_frequency();
int get_no_logical_processors();
std::vector<double> get_per_core_usage();

double get_cpu_usage()
{
    static long prev_idle = 0, prev_total = 0;
    static bool initialized = false;

    auto read_totals = []() -> std::pair<long, long>
    {
        std::ifstream file("/proc/stat");
        std::string line;
        if (!std::getline(file, line))
            return {0, 0};
        std::istringstream iss(line);
        std::string cpu;
        long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0, guest = 0, guest_nice = 0;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

        long idle_time = idle + iowait;
        long non_idle = user + nice + system + irq + softirq + steal;
        long total = idle_time + non_idle;
        return {idle_time, total};
    };

    auto [idle_time, total_time] = read_totals();

    if (!initialized)
    {
        prev_idle = idle_time;
        prev_total = total_time;
        initialized = true;

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::tie(idle_time, total_time) = read_totals();
    }

    long idle_diff = idle_time - prev_idle;
    long total_diff = total_time - prev_total;

    double usage = 0.0;
    if (total_diff > 0)
        usage = 100.0 * (1.0 - (double)idle_diff / (double)total_diff);

    prev_idle = idle_time;
    prev_total = total_time;

    return usage < 0.0 ? 0.0 : (usage > 100.0 ? 100.0 : usage);
}

std::string get_cpu_name()
{
    std::ifstream file("/proc/cpuinfo");
    std::string key;

    if (!file.is_open())
    {
        return "Error: Could not open /proc/cpuinfo.";
    }

    while (std::getline(file, key))
    {
        if (key.rfind("model name", 0) == 0)
        {
            size_t colon_pos = key.find(':');
            if (colon_pos != std::string::npos)
            {
                std::string cpu_name = key.substr(colon_pos + 1);
                size_t first_char_pos = cpu_name.find_first_not_of(" \t");
                if (first_char_pos != std::string::npos)
                {
                    return cpu_name.substr(first_char_pos);
                }
                return cpu_name;
            }
        }
    }
    return "Error: CPU model not found.";
}

long long get_running_processes()
{
    std::ifstream file("/proc/stat");
    std::string line;
    long long num_processes = 0;

    while (std::getline(file, line))
    {
        if (line.rfind("procs_running ", 0) == 0)
        {
            size_t pos = line.find(" ");
            if (pos != std::string::npos)
            {
                num_processes = std::stoll(line.substr(pos + 1));
                break;
            }
        }
    }

    file.close();
    if (num_processes <= 0)
    {
        return 0;
    }
    return num_processes;
}

double get_cpu_frequency()
{
    std::ifstream file("/proc/cpuinfo");
    std::string key;
    double mhz = 0.0;

    while (std::getline(file, key))
    {
        if (key.find("cpu MHz") == 0)
        {
            std::stringstream ss(key.substr(key.find(":") + 1));
            ss >> mhz;
            break;
        }
    }
    return mhz; // NOTE: value is in MHz, can mult by 0.001 for GHz
}

int get_no_logical_processors()
{
    std::ifstream file("/proc/cpuinfo");
    std::string key;
    int no_processors = 0;

    while (std::getline(file, key))
    {
        if (key.find("processor") == 0)
        {
            no_processors++;
        }
    }

    return no_processors;
}

std::vector<double> get_per_core_usage()
{
    static std::vector<long> prev_idle_times;
    static std::vector<long> prev_total_times;
    static bool first_call = true;

    std::ifstream file("/proc/stat");
    std::string line;
    std::vector<long> idle_times;
    std::vector<long> total_times;

    while (std::getline(file, line))
    {
        if (line.rfind("cpu", 0) == 0 && line != "cpu")
        {
            std::istringstream iss(line);
            std::string cpu_label;
            long user, nice, system, idle, iowait, irq, softirq, steal;

            if (!(iss >> cpu_label >> user >> nice >> system >> idle >>
                  iowait >> irq >> softirq >> steal))
                continue;

            if (cpu_label == "cpu")
                continue;

            long total_idle = idle + iowait;

            long total_non_idle = user + nice + system + irq + softirq + steal;

            long total = total_idle + total_non_idle;

            idle_times.push_back(total_idle);
            total_times.push_back(total);
        }
    }

    if (first_call || prev_idle_times.size() != idle_times.size())
    {
        prev_idle_times = idle_times;
        prev_total_times = total_times;
        first_call = false;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        return get_per_core_usage();
    }

    std::vector<double> usages(idle_times.size(), 0.0);
    for (size_t i = 0; i < idle_times.size() && i < prev_idle_times.size(); ++i)
    {
        long idle_diff = idle_times[i] - prev_idle_times[i];
        long total_diff = total_times[i] - prev_total_times[i];

        if (total_diff <= 0)
        {
            usages[i] = 0.0;
            continue;
        }

        double usage = 100.0 * (1.0 - (double)idle_diff / (double)total_diff);
        usages[i] = (usage < 0.0) ? 0.0 : (usage > 100.0 ? 100.0 : usage);
    }

    prev_idle_times = idle_times;
    prev_total_times = total_times;

    return usages;
}