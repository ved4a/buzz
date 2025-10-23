#include <memory.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

double get_memory_usage();
long get_mem_value(std::string mem_value);

double get_memory_usage()
{
    std::ifstream file("/proc/meminfo"); // enter memory stream
    std::string key, unit;
    long value = 0;
    double mem_total = 0, mem_available = 0; // link to resource: https://www.kernel.org/doc/html/v5.9/filesystems/proc.html

    while (file >> key >> value >> unit)
    {
        if (key == "MemTotal:")
            mem_total = value;
        if (key == "MemAvailable:")
            mem_available = value;
    }

    if (mem_total <= 0.0)
        return 0.0; // avoid division by zero

    double usage = 100.0 * (1.0 - (mem_available / mem_total));
    return usage;
}

long get_mem_value(std::string mem_value)
{
    std::ifstream file("/proc/meminfo");
    std::string key, unit;
    long value = 0;
    long desired_value = 0;

    while (file >> key >> value >> unit)
    {
        if (key == mem_value)
        {
            desired_value = value;
            break;
        }
    }
    return desired_value; // NOTE: this is in kb
}