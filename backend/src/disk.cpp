#include "disk.hpp"
#include <fstream>
#include <sstream>
#include <vector>

using json = nlohmann::json;

std::vector<DiskStats> get_disk_stats()
{
    std::vector<DiskStats> disks;
    std::ifstream file("/proc/diskstats");
    std::string line;

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        int major = 0, minor = 0;
        std::string dev;
        if (!(iss >> major >> minor >> dev))
            continue;

        // skip loopback & RAM disks
        if (dev.find("loop") != std::string::npos || dev.find("ram") != std::string::npos)
            continue;

        unsigned long long rd_ios = 0, rd_merged = 0, rd_sectors = 0, rd_time = 0;
        unsigned long long wr_ios = 0, wr_merged = 0, wr_sectors = 0, wr_time = 0;
        unsigned long long ios_in_prog = 0, io_time = 0, weighted_io_time = 0;

        if (!(iss >> rd_ios >> rd_merged >> rd_sectors >> rd_time >> wr_ios >> wr_merged >> wr_sectors >> wr_time >> ios_in_prog >> io_time >> weighted_io_time))
            continue;

        DiskStats d;
        d.device = dev;
        d.reads_completed = static_cast<long>(rd_ios);
        d.writes_completed = static_cast<long>(wr_ios);
        d.sectors_read = static_cast<long>(rd_sectors);
        d.sectors_written = static_cast<long>(wr_sectors);
        d.read_time_ms = static_cast<double>(rd_time);
        d.write_time_ms = static_cast<double>(wr_time);

        disks.push_back(d);
    }

    return disks;
};

json disk_to_json(const DiskStats &d)
{
    return {
        {"device", d.device},
        {"reads_completed", d.reads_completed},
        {"writes_completed", d.writes_completed},
        {"sectors_read", d.sectors_read},
        {"sectors_written", d.sectors_written},
        {"read_time_ms", d.read_time_ms},
        {"write_time_ms", d.write_time_ms}};
};