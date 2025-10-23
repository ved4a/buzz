#include "processes.hpp"
#include "memory.hpp"
#include "cpu.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <algorithm>
#include <signal.h>
#include <cerrno>
#include <cstring>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// HELPERS
std::string get_username_from_uid(uid_t uid)
{
    struct passwd *pwd = getpwuid(uid);
    return pwd ? std::string(pwd->pw_name) : "unknown";
}

std::string get_process_name(int pid)
{
    std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");
    std::string name;
    getline(comm, name);
    return name;
}

// per process, we wanna see which user the process is running for
uid_t get_process_uid(int pid)
{
    std::ifstream status("/proc/" + std::to_string(pid) + "/status");
    std::string key;
    uid_t uid = -1;
    while (status >> key)
    {
        if (key == "Uid:")
        {
            status >> uid;
            break;
        }
    }
    return uid;
}

// per process, we require memory usage
long get_process_memory_usage(int pid)
{
    // Try /proc/<pid>/status VmRSS
    std::ifstream status("/proc/" + std::to_string(pid) + "/status");
    long rss_kb = 0;

    if (status.is_open())
    {
        std::string line;
        while (std::getline(status, line))
        {
            if (line.rfind("VmRSS:", 0) == 0)
            {
                // After "VmRSS:" expect "<num> kB"
                std::istringstream iss(line.substr(6));
                long val = 0;
                std::string unit;
                if (iss >> val >> unit)
                {
                    rss_kb = val; // already kB
                }
                break;
            }
        }
    }

    // Fallback: /proc/<pid>/statm resident pages * page_size
    if (rss_kb == 0)
    {
        std::ifstream statm("/proc/" + std::to_string(pid) + "/statm");
        long size_pages = 0, resident_pages = 0;
        if (statm >> size_pages >> resident_pages)
        {
            long page_kb = sysconf(_SC_PAGESIZE) / 1024;
            rss_kb = resident_pages * page_kb;
        }
    }

    return rss_kb;
}

int get_thread_count(int pid)
{
    std::ifstream status("/proc/" + std::to_string(pid) + "/status");
    std::string key;
    int threads = 0;
    while (status >> key)
    {
        if (key == "Threads:")
        {
            status >> threads;
            break;
        }
    }
    return threads;
}

double get_process_cpu_time(int pid)
{
    std::ifstream statFile("/proc/" + std::to_string(pid) + "/stat");
    if (!statFile.is_open())
        return 0.0;

    std::string line;
    std::getline(statFile, line);
    if (line.empty())
        return 0.0;

    size_t rparen = line.rfind(')');
    if (rparen == std::string::npos)
        return 0.0;

    std::string after = line.substr(rparen + 2); // skip ") "
    std::istringstream iss(after);

    // field 3 is a char, not a number
    char stateChar = '\0';
    if (!(iss >> stateChar))
        return 0.0;

    // go over fields 4..15 as integers and pick 14/15 (utime/stime)
    long val = 0, utime = 0, stime = 0;
    for (int field = 4; field <= 15; ++field)
    {
        if (!(iss >> val))
            return 0.0;
        if (field == 14)
            utime = val;
        else if (field == 15)
            stime = val;
    }

    long ticks = sysconf(_SC_CLK_TCK);
    return ticks > 0 ? static_cast<double>(utime + stime) / static_cast<double>(ticks) : 0.0;
}

std::string get_process_status(int pid)
{
    static const std::unordered_map<std::string, std::string> status_map = {
        {"R", "Running"},
        {"S", "Sleeping"},
        {"Z", "Zombie"},
        {"T", "Traced or Stopped"},
        {"D", "Sleeping, Uninterruptable"}};

    std::ifstream status("/proc/" + std::to_string(pid) + "/status");
    std::string key, process_status = "Unknown";

    while (status >> key)
    {
        if (key == "State:")
        {
            status >> process_status;
            break;
        }
    }

    auto it = status_map.find(process_status);
    return (it != status_map.end()) ? it->second : "Unknown";
}

static long read_total_jiffies()
{
    std::ifstream f("/proc/stat");
    std::string line;
    if (!std::getline(f, line))
        return 0;
    std::istringstream iss(line);
    std::string cpu;
    long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0, guest = 0, guest_nice = 0;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
    return user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
}

static long read_process_jiffies(int pid)
{
    std::ifstream statFile("/proc/" + std::to_string(pid) + "/stat");
    if (!statFile.is_open())
        return 0;
    std::string line;
    std::getline(statFile, line);
    if (line.empty())
        return 0;

    size_t rparen = line.rfind(')');
    if (rparen == std::string::npos)
        return 0;

    std::string after = line.substr(rparen + 2); // skip ") "
    std::istringstream iss(after);

    // field 3 is a char
    char stateChar = '\0';
    if (!(iss >> stateChar))
        return 0;

    long val = 0, utime = 0, stime = 0;
    for (int field = 4; field <= 15; ++field)
    {
        if (!(iss >> val))
            return 0;
        if (field == 14)
            utime = val;
        else if (field == 15)
            stime = val;
    }
    return utime + stime;
}

bool kill_process(int pid, int sig, std::string *error_msg)
{
    // check if process exists
    if (::kill(pid, 0) == -1)
    {
        if (error_msg)
            *error_msg = std::string("Process not available: ") + std::strerror(errno);
        return false;
    }
    if (::kill(pid, sig) == -1)
    {
        if (error_msg)
            *error_msg = std::string("Failed to send signal: ") + std::strerror(errno);
        return false;
    }
    return true;
}

// MAIN
std::vector<ProcessInfo> get_all_processes()
{
    std::vector<ProcessInfo> processes;

    // t0
    long total0 = read_total_jiffies();
    std::unordered_map<int, long> proc0;

    for (const auto &entry : fs::directory_iterator("/proc"))
    {
        if (!entry.is_directory())
            continue;

        std::string dirname = entry.path().filename();
        if (!std::all_of(dirname.begin(), dirname.end(), ::isdigit))
            continue;

        int pid = std::stoi(dirname);

        ProcessInfo p;
        p.pid = pid;
        p.process_name = get_process_name(pid);
        p.status = get_process_status(pid);
        p.memory_usage = get_process_memory_usage(pid); // kB (with fallback)
        p.threads = get_thread_count(pid);
        p.cpu.cpu_time = get_process_cpu_time(pid); // seconds (safe parse)
        p.cpu.cpu_usage = 0.0;                      // filled after t1
        p.user = get_username_from_uid(get_process_uid(pid));

        p.type = (p.user == "root" || p.process_name.find('d') != std::string::npos)
                     ? "background process"
                     : "app";

        processes.push_back(p);
        proc0[pid] = read_process_jiffies(pid);
    }

    // longer window helps on idle systems/WSL
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // t1
    long total1 = read_total_jiffies();
    long delta_total = total1 - total0;
    if (delta_total < 1)
        delta_total = 1;

    std::vector<ProcessInfo> alive;
    alive.reserve(processes.size());

    // MemTotal for memory%
    long mem_total_kb = get_mem_value("MemTotal:");
    if (mem_total_kb <= 0)
    {
        // fallback to /proc/meminfo if memory.hpp couldn’t read it
        std::ifstream meminfo("/proc/meminfo");
        std::string key, unit;
        long val = 0;
        while (meminfo >> key >> val >> unit)
        {
            if (key == "MemTotal:")
            {
                mem_total_kb = val;
                break;
            }
        }
    }

    long ticks = sysconf(_SC_CLK_TCK);

    for (auto &p : processes)
    {
        if (!fs::exists("/proc/" + std::to_string(p.pid)))
        {
            continue; // skip exited processes
        }

        long j0 = 0;
        if (auto it = proc0.find(p.pid); it != proc0.end())
            j0 = it->second;

        long j1 = read_process_jiffies(p.pid);
        long delta_proc = j1 - j0;

        // if process exited between t0 and t1, keep zeros
        if (delta_proc < 0)
            delta_proc = 0;

        long delta_total = total1 - total0;
        if (delta_total < 1)
            delta_total = 1;

        // CPU% snapshot over the interval
        const int ncpu = get_no_logical_processors(); // scale by # processors
        double cpu_pct = 100.0 * static_cast<double>(delta_proc) / static_cast<double>(delta_total) * static_cast<double>(ncpu > 0 ? ncpu : 1);
        if (cpu_pct < 0.0)
            cpu_pct = 0.0;
        if (cpu_pct > 100.0)
            cpu_pct = 100.0;

        p.cpu.cpu_usage = cpu_pct;
        p.cpu.cpu_time = (ticks > 0) ? static_cast<double>(j1) / static_cast<double>(ticks) : 0.0;

        // Memory%
        p.memory_percent = (mem_total_kb > 0)
                               ? 100.0 * static_cast<double>(p.memory_usage) / static_cast<double>(mem_total_kb)
                               : 0.0;

        alive.push_back(std::move(p));
    }

    return alive;
}

json process_to_json(const ProcessInfo &p)
{
    json j;
    j["type"] = p.type;
    j["process_id"] = p.pid;
    j["process_name"] = p.process_name;
    j["user"] = p.user;
    j["status"] = p.status;

    j["cpu"] = {
        {"cpu_usage", p.cpu.cpu_usage}, // %
        {"cpu_time", p.cpu.cpu_time}    // seconds (cumulative)
    };

    j["memory"] = {
        {"memory_usage_kb", p.memory_usage},
        {"memory_percent", p.memory_percent}};

    j["threads"] = p.threads;

    return j;
}