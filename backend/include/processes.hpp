#ifndef PROCESSES_HPP
#define PROCESSES_HPP

#include <csignal>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct CPUInfo
{
    double cpu_usage; // % CPU used by the process (interval based)
    double cpu_time;  // Cumulative CPU time (seconds)
};

struct ProcessInfo
{
    int pid;                  // process ID
    std::string process_name; // name of executable
    std::string type;         // "background" or "app"
    CPUInfo cpu;
    long memory_usage;     // kB (VmRSS)
    double memory_percent; // % of MemTotal
    std::string status;
    int threads;
    std::string user; // username
};

// collect all running processes on linux
std::vector<ProcessInfo> get_all_processes();

// convert process info to JSON format
nlohmann::json process_to_json(const ProcessInfo &p);

// kill a process by pid, return true on success
bool kill_process(int pid, int sig = SIGTERM, std::string *error_msg = nullptr);

#endif