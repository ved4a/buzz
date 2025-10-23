#include "cli.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <csignal>
#include <cstdlib>
#include "processes.hpp"

using nlohmann::json;

namespace
{
    bool parse_int(const std::string &s, int &out)
    {
        try
        {
            size_t idx = 0;
            int v = std::stoi(s, &idx, 10);
            if (idx != s.size())
                return false;
            out = v;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

std::optional<int> cli::run(int argc, char **argv)
{
    if (argc < 2)
        return std::nullopt;

    const std::string cmd = argv[1];

    if (cmd == "--kill" || cmd == "kill")
    {
        json j;
        j["action"] = "kill";

        if (argc < 3)
        {
            j["success"] = false;
            j["error"] = "Usage: --kill <pid> [--force|--sigkill|--sigterm|--signal <num>]";
            std::cout << j.dump(4) << std::endl;
            return 2;
        }

        int pid = 0;
        if (!parse_int(argv[2], pid))
        {
            j["success"] = false;
            j["error"] = "Invalid PID";
            std::cout << j.dump(4) << std::endl;
            return 2;
        }

        // safety: avoid killing 0/1 (init) and negative pids (process groups)
        if (pid <= 1)
        {
            j["success"] = false;
            j["pid"] = pid;
            j["error"] = "Refusing to signal PID <= 1";
            std::cout << j.dump(4) << std::endl;
            return 2;
        }

        int sig = SIGTERM;
        // parse opt signal flag
        for (int i = 3; i < argc; ++i)
        {
            const std::string opt = argv[i];
            if (opt == "--force" || opt == "--sigkill")
                sig = SIGKILL;
            else if (opt == "--sigterm")
                sig = SIGTERM;
            else if (opt == "--signal" && i + 1 < argc)
            {
                int s = 0;
                if (parse_int(argv[i + 1], s))
                {
                    sig = s;
                    ++i;
                }
            }
        }

        std::string err;
        bool ok = kill_process(pid, sig, &err);

        j["pid"] = pid;
        j["signal"] = sig;
        j["success"] = ok;
        if (!ok)
            j["error"] = err;

        std::cout << j.dump(4) << std::endl;
        return ok ? 0 : 1;
    }

    // no known subcommand; let main do the default snapshot
    return std::nullopt;
}