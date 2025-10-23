#ifndef SNAPSHOT_HPP
#define SNAPSHOT_HPP

#include <string>
#include <nlohmann/json.hpp>

namespace snapshot
{
    // structure mirrors the output of src/main.cpp:
    nlohmann::json make();

    // generate a default filename for saving the snapshot
    // eg: buzz-snapshot-20250101-123045Z.json
    std::string default_filename();

    bool save_to_file(const nlohmann::json &j, const std::string &path, std::string *err = nullptr);
}

#endif
