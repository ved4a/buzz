#ifndef CPU_HPP
#define CPU_HPP
#include <string>
#include <vector>

double get_cpu_usage();
std::string get_cpu_name();
long long get_running_processes();
double get_cpu_frequency();
int get_no_logical_processors();
std::vector<double> get_per_core_usage();

#endif
