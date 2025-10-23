#pragma once
#include <optional>

namespace cli
{
    // processes a CLI subcommand: return the exit code to use and prints JSON to stdout
    // doesn't recognize any subcommand: return std::nullopt so main can do the default work
    std::optional<int> run(int argc, char **argv);
}