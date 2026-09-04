#pragma once
#include <string>
#include <vector>

namespace Cheats
{
    void Menu();
    void Run();

    // Optional UI test hook. Live spectator data is populated during Cheats::Run().
    void SetSpectatorPreviewNames(const std::vector<std::string>& names);
}
