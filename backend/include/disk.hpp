#ifndef DISK_HPP
#define DISK_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct DiskStats
{
    std::string device;
    long reads_completed;
    long writes_completed;
    long sectors_read;
    long sectors_written;
    double read_time_ms;
    double write_time_ms;
};

std::vector<DiskStats> get_disk_stats();
nlohmann::json disk_to_json(const DiskStats &d);

#endif