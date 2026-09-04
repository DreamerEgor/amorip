#pragma once
#include <string>

namespace MyConfigSaver
{
    bool SaveConfig(const std::string& filename);
    bool LoadConfig(const std::string& filename);
}
