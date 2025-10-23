#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>
#include <map>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <sys/select.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <filesystem>

#include <nlohmann/json.hpp>

#include <cpu.hpp>
#include <memory.hpp>
#include <processes.hpp>
#include <disk.hpp>
#include <network.hpp>
#include <battery.hpp>
#include <snapshot.hpp>

using json = nlohmann::json;

// signal handling
static bool running = true;

static void on_signal(int)
{
    running = false;
}

// colors
namespace ansi
{
    // clear entire screen + scrollback and move cursor home
    static const char *clear = "\033[H\033[2J\033[3J";
    static const char *home = "\033[H";
    static const char *hide_cursor = "\033[?25l";
    static const char *show_cursor = "\033[?25h";

    struct Theme
    {
        std::string reset = "\033[0m";
        std::string dim = "\033[2m";
        std::string header = "\033[1;36m"; // cyan
        std::string title = "\033[1;35m";  // magenta
        std::string ok = "\033[1;32m";     // green
        std::string warn = "\033[1;33m";   // yellow
        std::string err = "\033[1;31m";    // red
        bool enabled = true;

        std::string on(bool enabled, const std::string &code)
        {
            return enabled ? code : "";
        }
    };

}
static ansi::Theme theme;

// config options
struct Options
{
    int refresh_ms = 2000;
    bool no_color = false;
    std::string sort = "cpu"; // options being cpu or mem
    int top = 25;
};

// flatten json from main.cpp
static void flatten_json(const json &j, const std::string &prefix, std::map<std::string, std::string> &out)
{
    if (j.is_object())
    {
        for (auto it = j.begin(); it != j.end(); ++it)
        {
            std::string key = prefix.empty() ? it.key() : (prefix + "." + it.key());
            flatten_json(it.value(), key, out);
        }
    }
    else if (j.is_array())
    {
        if (j.empty())
        {
            out[prefix] = "[]";
        }
        else
        {
            // expand array w indices
            // eg: network.interfaces[0].interface = "lo
            for (size_t i = 0; i < j.size(); ++i)
            {
                std::string key = prefix + "[" + std::to_string(i) + "]";
                flatten_json(j[i], key, out);
            }
        }
    }
    else if (j.is_string())
    {
        out[prefix] = j.get<std::string>();
    }
    else if (j.is_number_integer())
    {
        out[prefix] = std::to_string(j.get<long long>());
    }
    else if (j.is_number_unsigned())
    {
        out[prefix] = std::to_string(j.get<unsigned long long>());
    }
    else if (j.is_number_float())
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << j.get<double>();
        out[prefix] = oss.str();
    }
    else if (j.is_boolean())
    {
        out[prefix] = j.get<bool>() ? "true" : "false";
    }
    else if (j.is_null())
    {
        out[prefix] = "null";
    }
}

// 4.2013 -> "4.2%"
static std::string fmt_pct(double v)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << v << "%";
    return oss.str();
}

// bps to human-readable form w/ corresponding unit!
// 1048576.0 -> "1.00 MB/s"
static std::string human_bytes(double bps)
{
    const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    int idx = 0;
    while (bps >= 1024.0 && idx < 4)
    {
        bps /= 1024.0;
        ++idx;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(bps >= 100 ? 0 : 1) << bps << " " << units[idx];
    return oss.str();
}

// same but byte COUNTS
// 8192.0 -> "8.0 KB"
static std::string human_bytes_total(double bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    while (bytes >= 1024.0 && idx < 4)
    {
        bytes /= 1024.0;
        ++idx;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(bytes >= 100 ? 0 : 1) << bytes << " " << units[idx];
    return oss.str();
}

// print a thin gray line for dividing tables
static void print_line()
{
    // adapt to terminal width
    winsize ws{};
    int width = 120;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        width = ws.ws_col;
    // leave a tiny margin
    int line_len = std::max(20, width - 2);
    std::cout << theme.on(theme.enabled, theme.dim) << std::string(line_len, '-') << theme.on(theme.enabled, theme.reset) << "\n";
}

// clean formatting of key-value pairs
static void print_kv(const std::vector<std::pair<std::string, std::string>> &rows)
{
    size_t kmax = 0;
    for (auto &kv : rows)
        kmax = std::max(kmax, kv.first.size());
    for (auto &kv : rows)
    {
        std::cout << "  " << theme.on(theme.enabled, theme.header) << std::setw((int)kmax) << std::left << kv.first
                  << theme.on(theme.enabled, theme.reset) << " : " << kv.second << "\n";
    }
}

// truncate to fit column width
static inline std::string ellipsize(const std::string &s, size_t maxw)
{
    if (maxw == 0)
        return "";
    if (s.size() <= maxw)
        return s;
    if (maxw <= 3)
        return s.substr(0, maxw);
    return s.substr(0, maxw - 3) + "...";
}

static int get_terminal_width()
{
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 120;
}

static void print_table(const std::string &title, const std::vector<json> &rows, const std::vector<std::string> &preferred_cols = {}, int max_rows = 25)
{
    std::cout << theme.on(theme.enabled, theme.title) << title << theme.on(theme.enabled, theme.reset) << "\n";
    if (rows.empty())
    {
        std::cout << "  (no data)\n";
        return;
    }

    // flatten rows
    std::vector<std::map<std::string, std::string>> flat;
    flat.reserve(rows.size());
    std::set<std::string> allcols;
    for (const auto &r : rows)
    {
        std::map<std::string, std::string> f;
        flatten_json(r, "", f);
        flat.push_back(std::move(f));
        for (auto &kv : flat.back())
            allcols.insert(kv.first);
    }

    // determine column order
    // if preference exists, print that
    // if not, print alphabetically
    std::vector<std::string> cols;
    std::set<std::string> used;
    for (const auto &c : preferred_cols)
    {
        if (allcols.count(c))
        {
            cols.push_back(c);
            used.insert(c);
        }
    }
    for (const auto &c : allcols)
    {
        if (!used.count(c))
            cols.push_back(c);
    }

    // find max column width & assign that as all widths
    // clamp widths and adapt to terminal width
    std::vector<size_t> widths(cols.size(), 0);
    for (size_t i = 0; i < cols.size(); ++i)
    {
        widths[i] = std::max(widths[i], cols[i].size());
    }
    int rcount = std::min<int>((int)flat.size(), max_rows);
    for (int r = 0; r < rcount; ++r)
    {
        for (size_t i = 0; i < cols.size(); ++i)
        {
            auto it = flat[r].find(cols[i]);
            if (it != flat[r].end())
                widths[i] = std::max(widths[i], it->second.size());
        }
    }

    // clamp individual column widths
    const size_t MAX_COL_WIDTH = 40;
    const size_t MIN_COL_WIDTH = 6;
    for (size_t i = 0; i < widths.size(); ++i)
    {
        if (widths[i] > MAX_COL_WIDTH)
            widths[i] = MAX_COL_WIDTH;
        if (widths[i] < MIN_COL_WIDTH)
            widths[i] = std::max(widths[i], MIN_COL_WIDTH);
    }

    // drop columns to fit terminal width if necessary
    int termw = get_terminal_width();
    int padding = 2;         // left margin spaces before the table
    int inter_col_space = 2; // spaces between columns
    auto required_width = [&](size_t count) -> size_t
    {
        if (count == 0)
            return 0;
        size_t sum = 0;
        for (size_t i = 0; i < count; ++i)
            sum += widths[i];
        sum += (count - 1) * inter_col_space;
        sum += padding; // left margin
        return sum;
    };

    // keep as many columns as will fit
    size_t keep = cols.size();
    while (keep > 0 && (int)required_width(keep) > std::max(40, termw - 2))
        --keep;
    if (keep < cols.size())
    {
        cols.resize(keep);
        widths.resize(keep);
    }

    // header is left-aligned, in cyan, 2 spaces per column
    std::cout << "  ";
    for (size_t i = 0; i < cols.size(); ++i)
    {
        std::string head = ellipsize(cols[i], widths[i]);
        std::cout << theme.on(theme.enabled, theme.header) << std::setw((int)widths[i]) << std::left << head << theme.on(theme.enabled, theme.reset);
        if (i + 1 < cols.size())
            std::cout << "  ";
    }
    std::cout << "\n";
    print_line();

    // prints upto max rows for data rows
    // left aligned
    for (int r = 0; r < rcount; ++r)
    {
        std::cout << "  ";
        for (size_t i = 0; i < cols.size(); ++i)
        {
            auto it = flat[r].find(cols[i]);
            std::string val = (it != flat[r].end()) ? it->second : "";
            val = ellipsize(val, widths[i]);
            std::cout << std::setw((int)widths[i]) << std::left << val;
            if (i + 1 < cols.size())
                std::cout << "  ";
        }
        std::cout << "\n";
    }
}

static void usage(const char *argv0)
{
    std::cout << "Usage: " << argv0 << " [--refresh <ms>] [--no-color] [--sort cpu|mem] [--top N]\n";
}

static Options parse_opts(int argc, char **argv)
{
    Options o; // initialize w default options
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--refresh" && i + 1 < argc)
        {
            o.refresh_ms = std::max(250, std::atoi(argv[++i]));
        }
        else if (a == "--no-color")
        {
            o.no_color = true;
        }
        else if (a == "--sort" && i + 1 < argc)
        {
            o.sort = argv[++i];
            if (o.sort != "cpu" && o.sort != "mem")
                o.sort = "cpu";
        }
        else if (a == "--top" && i + 1 < argc)
        {
            o.top = std::max(1, std::atoi(argv[++i]));
        }
        else if (a == "-h" || a == "--help")
        {
            usage(argv[0]);
            std::exit(0);
        }
    }
    return o;
}

int main(int argc, char **argv)
{
    auto opts = parse_opts(argc, argv);
    theme.enabled = !opts.no_color;

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::cout << ansi::hide_cursor;

    while (running)
    {
        auto t0 = std::chrono::steady_clock::now();

        // CPU summary
        json cpu_json;
        cpu_json["cpu_usage"] = get_cpu_usage();
        cpu_json["cpu_name"] = get_cpu_name();
        cpu_json["running_processes"] = get_running_processes();
        cpu_json["cpu_frequency"] = get_cpu_frequency();
        cpu_json["no_of_logical_processors"] = get_no_logical_processors();

        // memory summary
        json mem_json;
        mem_json["memory_usage"] = get_memory_usage();
        mem_json["cached_memory"] = get_mem_value("Cached:");
        mem_json["free_swappable_memory"] = get_mem_value("SwapFree:");
        mem_json["total_swappable_memory"] = get_mem_value("SwapTotal:");
        // additional totals in bytes if available via /proc/meminfo keys
        // We reuse get_mem_value which returns KiB for matching labels.
        long mem_total_kib = get_mem_value("MemTotal:");
        long mem_free_kib = get_mem_value("MemAvailable:");
        if (mem_total_kib > 0)
            mem_json["total_memory_kib"] = mem_total_kib;
        if (mem_free_kib > 0)
            mem_json["available_memory_kib"] = mem_free_kib;

        // battery stats
        auto b = get_battery_info();
        json batt_json = battery_to_json(b);

        // processes
        auto processes = get_all_processes();
        std::vector<json> proc_rows;
        proc_rows.reserve(processes.size());
        for (const auto &p : processes)
            proc_rows.push_back(process_to_json(p));

        // sort processes
        auto get_double = [](const json &r, const std::string &key, double defv = 0.0)
        {
            const json *cur = &r;
            std::stringstream ss(key);
            std::string part;
            while (std::getline(ss, part, '.'))
            {
                if (!cur->is_object() || !cur->contains(part))
                    return defv;
                cur = &((*cur)[part]);
            }
            if (cur->is_number_float())
                return cur->get<double>();
            if (cur->is_number_integer())
                return static_cast<double>(cur->get<long long>());
            if (cur->is_number_unsigned())
                return static_cast<double>(cur->get<unsigned long long>());
            return defv;
        };
        std::string sort_key = (opts.sort == "mem") ? "memory.memory_percent" : "cpu.cpu_usage";
        std::sort(proc_rows.begin(), proc_rows.end(), [&](const json &a, const json &b)
                  { return get_double(a, sort_key) > get_double(b, sort_key); });

        // network
        auto net_ifaces = get_network_rates();
        std::vector<json> net_rows;
        net_rows.reserve(net_ifaces.size());
        for (const auto &n : net_ifaces)
            net_rows.push_back(network_to_json(n));

        // disks
        auto disks = get_disk_stats();
        std::vector<json> disk_rows;
        disk_rows.reserve(disks.size());
        for (const auto &d : disks)
            disk_rows.push_back(disk_to_json(d));

        // render w color!
        std::cout << ansi::clear << ansi::home << std::flush;

        // title
        std::cout << theme.on(theme.enabled, theme.ok) << std::string("buzz: a lightweight resource monitor")
                  << theme.on(theme.enabled, theme.reset) << "  "
                  << theme.on(theme.enabled, theme.dim) << "(configure with --refresh <ms> --sort <cpu|mem> --top <N>)"
                  << theme.on(theme.enabled, theme.reset) << "\n";
        print_line();

        // summary
        std::vector<std::pair<std::string, std::string>> kv;
        kv.push_back({"CPU", fmt_pct(cpu_json["cpu_usage"].get<double>())});
        kv.push_back({"CPU Freq (GHz)", [&]()
                      {
                          std::ostringstream s;
                          s << std::fixed << std::setprecision(2) << cpu_json["cpu_frequency"].get<double>();
                          return s.str();
                      }()});
        kv.push_back({"Procs Running", std::to_string((long long)cpu_json["running_processes"].get<long long>())});
        kv.push_back({"Cores", std::to_string(cpu_json["no_of_logical_processors"].get<int>())});

        kv.push_back({"Memory Used", fmt_pct(mem_json["memory_usage"].get<double>())});
        kv.push_back({"Swap Free", human_bytes_total(1024.0 * mem_json["free_swappable_memory"].get<long>())});
        kv.push_back({"Swap Total", human_bytes_total(1024.0 * mem_json["total_swappable_memory"].get<long>())});
        if (mem_json.contains("total_memory_kib"))
            kv.push_back({"Mem Total", human_bytes_total(1024.0 * mem_json["total_memory_kib"].get<long>())});
        if (mem_json.contains("available_memory_kib"))
            kv.push_back({"Mem Avail", human_bytes_total(1024.0 * mem_json["available_memory_kib"].get<long>())});

        kv.push_back({"Battery", batt_json["status"].get<std::string>() + " (" + std::to_string(batt_json["current_capacity"].get<int>()) + "%)"});
        kv.push_back({"Refresh", std::to_string(opts.refresh_ms) + " ms"});

        print_kv(kv);
        print_line();

        // processes table (top N)
        {
            // Prefer showing the sort-related columns earlier. When sorting by memory,
            // place memory columns before CPU columns so they are less likely to be
            // trimmed on narrow terminals.
            std::vector<std::string> pref;
            if (opts.sort == "mem")
            {
                pref = {
                    "process_id", "process_name", "user", "status", "threads", "type",
                    // memory-first when sorting by memory
                    "memory.memory_percent", "memory.memory_usage_kb",
                    // then cpu
                    "cpu.cpu_usage", "cpu.cpu_time"};
            }
            else
            {
                // default (CPU-first)
                pref = {
                    "process_id", "process_name", "user", "status", "threads", "type",
                    "cpu.cpu_usage", "cpu.cpu_time", "memory.memory_percent", "memory.memory_usage_kb"};
            }
            print_table(std::string("Processes (sorted by ") + (opts.sort == "mem" ? "Memory%" : "CPU%") + ", top " + std::to_string(opts.top) + ")",
                        proc_rows, pref, opts.top);
        }
        print_line();

        // per-core CPU table
        {
            std::vector<double> per_core = get_per_core_usage();
            std::vector<json> core_rows;
            core_rows.reserve(per_core.size());
            for (size_t i = 0; i < per_core.size(); ++i)
            {
                json row;
                row["core"] = (int)i;
                row["usage_percent"] = per_core[i];
                core_rows.push_back(row);
            }
            print_table(std::string("CPU Cores"), core_rows, {"core", "usage_percent"}, 128);
        }
        print_line();

        // network table (humanized rates if fields present)
        // Try to replace numeric rates with human-readable strings if keys look like *_rate or *_bytes
        {
            std::vector<json> net_rows_human = net_rows;
            for (auto &row : net_rows_human)
            {
                for (auto it = row.begin(); it != row.end(); ++it)
                {
                    const std::string k = it.key();
                    if ((k.find("rate") != std::string::npos || k.find("bytes") != std::string::npos) && it.value().is_number())
                    {
                        double v = it.value().get<double>();
                        row[k] = human_bytes(v);
                    }
                }
            }
            print_table(std::string("Network Interfaces"), net_rows_human, {"interface", "download_rate", "upload_rate"});
        }
        print_line();

        // disk table (humanize *_bytes fields)
        {
            std::vector<json> disk_rows_human = disk_rows;
            for (auto &row : disk_rows_human)
            {
                for (auto it = row.begin(); it != row.end(); ++it)
                {
                    const std::string k = it.key();
                    if (k.find("bytes") != std::string::npos && it.value().is_number())
                    {
                        double v = it.value().get<double>();
                        row[k] = human_bytes_total(v);
                    }
                    if (k.find("rate") != std::string::npos && it.value().is_number())
                    {
                        double v = it.value().get<double>();
                        row[k] = human_bytes(v);
                    }
                }
            }
            print_table(std::string("Disks"), disk_rows_human);
        }
        print_line();

        // cmd prompt (non-blocking)
        std::cout << theme.on(theme.enabled, theme.warn) << "Command" << theme.on(theme.enabled, theme.reset)
                  << " [q to exit | d to download snapshot | k <pid> [--sigkill|--sigterm|--signal <num>] to kill processes]: " << std::flush;

        // wait while listening for input
        auto t1 = std::chrono::steady_clock::now();
        int elapsed_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        int wait_ms = std::max(0, opts.refresh_ms - elapsed_ms);

        // select() for stdin readiness
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        timeval tv;
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = (wait_ms % 1000) * 1000;

        int ready = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &rfds))
        {
            std::string line;
            if (std::getline(std::cin, line))
            {
                // trim
                auto ltrim = [](std::string &s)
                { s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c)
                                                  { return !std::isspace(c); })); };
                auto rtrim = [](std::string &s)
                { s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c)
                                       { return !std::isspace(c); })
                              .base(),
                          s.end()); };
                ltrim(line);
                rtrim(line);
                if (line == "q" || line == "quit" || line == "exit")
                {
                    break;
                }
                else if (!line.empty())
                {
                    // parse: k <pid> [--sigkill|--sigterm|--signal <num>]
                    std::istringstream iss(line);
                    std::string cmd;
                    iss >> cmd;
                    if (cmd == "d")
                    {
                        auto j = snapshot::make();
                        std::string path = snapshot::default_filename();
                        std::string err;
                        bool ok = snapshot::save_to_file(j, path, &err);
                        std::string display_path;
                        try
                        {
                            display_path = (std::filesystem::current_path() / path).string();
                        }
                        catch (...)
                        {
                            display_path = path; // fallback
                        }
                        std::cout << "\n"
                                  << (ok ? theme.on(theme.enabled, theme.ok) : theme.on(theme.enabled, theme.err))
                                  << (ok ? "Saved snapshot: " : "Snapshot failed: ")
                                  << theme.on(theme.enabled, theme.reset)
                                  << (ok ? display_path : err) << "\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(900));
                    }
                    else if (cmd == "k" || cmd == "kill")
                    {
                        int pid = 0;
                        iss >> pid;
                        int sig = SIGTERM;
                        std::string opt;
                        while (iss >> opt)
                        {
                            if (opt == "--sigkill" || opt == "--force")
                                sig = SIGKILL;
                            else if (opt == "--sigterm")
                                sig = SIGTERM;
                            else if (opt == "--signal")
                            {
                                int s = 0;
                                if (iss >> s)
                                    sig = s;
                            }
                        }
                        if (pid > 1)
                        {
                            std::string err;
                            bool ok = kill_process(pid, sig, &err);
                            std::cout << "\n"
                                      << (ok ? theme.on(theme.enabled, theme.ok) : theme.on(theme.enabled, theme.err))
                                      << (ok ? "OK" : "ERR") << theme.on(theme.enabled, theme.reset)
                                      << ": kill(" << pid << ", " << sig << ") "
                                      << (ok ? "success" : err) << "\n";
                            std::this_thread::sleep_for(std::chrono::milliseconds(900));
                        }
                        else
                        {
                            std::cout << "\n"
                                      << theme.on(theme.enabled, theme.warn) << "Refusing to signal PID <= 1" << theme.on(theme.enabled, theme.reset) << "\n";
                            std::this_thread::sleep_for(std::chrono::milliseconds(900));
                        }
                    }
                }
            }
            else
            {
                // EOF on stdin
                running = false;
            }
        }
    }

    std::cout << ansi::show_cursor << theme.on(theme.enabled, theme.reset);
    return 0;
}
