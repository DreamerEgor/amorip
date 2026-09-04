#include "ConfigSaver.hpp"
#include "../MenuConfig.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace
{
    std::filesystem::path FullPath(const std::string& filename)
    {
        return std::filesystem::path(MenuConfig::path) / filename;
    }

    void WriteColor(std::ofstream& f, const char* name, const ImColor& c)
    {
        f << name << ' ' << c.Value.x << ' ' << c.Value.y << ' ' << c.Value.z << ' ' << c.Value.w << '\n';
    }

    void WriteBind(std::ofstream& f, const char* name, const Keybind::Bind& b)
    {
        f << name << ' ' << b.key << ' ' << static_cast<int>(b.mode) << '\n';
    }

    bool ParseBool(const std::string& s)
    {
        return s == "1" || s == "true" || s == "True";
    }
}

bool MyConfigSaver::SaveConfig(const std::string& filename)
{
    std::error_code ec;
    std::filesystem::create_directories(MenuConfig::path, ec);
    std::ofstream f(FullPath(filename), std::ios::trunc);
    if (!f)
        return false;

    f << "version 1\n";
    f << "ESP " << MenuConfig::ESP << '\n';
    f << "TeamCheck " << MenuConfig::TeamCheck << '\n';
    f << "ShowBoxESP " << MenuConfig::ShowBoxESP << '\n';
    f << "ShowBoneESP " << MenuConfig::ShowBoneESP << '\n';
    f << "ShowHealthBar " << MenuConfig::ShowHealthBar << '\n';
    f << "ShowWeaponESP " << MenuConfig::ShowWeaponESP << '\n';
    f << "ShowDistance " << MenuConfig::ShowDistance << '\n';
    f << "ShowPlayerName " << MenuConfig::ShowPlayerName << '\n';
    WriteColor(f, "BoxColor", MenuConfig::BoxColor);
    WriteColor(f, "BoneColor", MenuConfig::BoneColor);
    WriteColor(f, "TextColor", MenuConfig::TextColor);

    f << "AimBot " << MenuConfig::AimBot << '\n';
    WriteBind(f, "AimKey", MenuConfig::AimKey);
    f << "AimFov " << MenuConfig::AimFov << '\n';
    f << "AimSmooth " << MenuConfig::AimSmooth << '\n';
    f << "AimPosition " << MenuConfig::AimPosition << '\n';
    f << "AimSpottedOnly " << MenuConfig::AimSpottedOnly << '\n';
    f << "ShowAimFovRange " << MenuConfig::ShowAimFovRange << '\n';
    WriteColor(f, "AimFovRangeColor", MenuConfig::AimFovRangeColor);

    f << "RCS " << MenuConfig::RCS << '\n';
    f << "RCSStartBullet " << MenuConfig::RCSStartBullet << '\n';
    f << "RCSSmooth " << MenuConfig::RCSSmooth << '\n';
    f << "RCSPitch " << MenuConfig::RCSPitch << '\n';
    f << "RCSYaw " << MenuConfig::RCSYaw << '\n';
    f << "RCSWeaponAware " << MenuConfig::RCSWeaponAware << '\n';

    f << "TriggerBot " << MenuConfig::TriggerBot << '\n';
    WriteBind(f, "TriggerKey", MenuConfig::TriggerKey);
    f << "TriggerDelay " << MenuConfig::TriggerDelay << '\n';
    f << "TriggerHitchance " << MenuConfig::TriggerHitchance << '\n';
    f << "TriggerSnipersScopedOnly " << MenuConfig::TriggerSnipersScopedOnly << '\n';
    f << "TriggerHitboxHead " << MenuConfig::TriggerHitboxes[0] << '\n';
    f << "TriggerHitboxNeck " << MenuConfig::TriggerHitboxes[1] << '\n';
    f << "TriggerHitboxChest " << MenuConfig::TriggerHitboxes[2] << '\n';
    f << "TriggerHitboxStomach " << MenuConfig::TriggerHitboxes[3] << '\n';
    f << "TriggerHitboxPelvis " << MenuConfig::TriggerHitboxes[4] << '\n';

    f << "BunnyHop " << MenuConfig::BunnyHop << '\n';
    WriteBind(f, "BunnyHopKey", MenuConfig::BunnyHopKey);

    WriteBind(f, "MenuKey", MenuConfig::MenuKey);
    WriteBind(f, "PanicKey", MenuConfig::PanicKey);
    WriteColor(f, "MenuAccentColor", MenuConfig::MenuAccentColor);
    f << "ShowStatusCounter " << MenuConfig::ShowStatusCounter << '\n';
    f << "StatusShowBrand " << MenuConfig::StatusShowBrand << '\n';
    f << "StatusShowUser " << MenuConfig::StatusShowUser << '\n';
    f << "StatusShowFps " << MenuConfig::StatusShowFps << '\n';
    f << "StatusShowPing " << MenuConfig::StatusShowPing << '\n';
    f << "ShowSpectatorList " << MenuConfig::ShowSpectatorList << '\n';
    f << "OverlayFpsLimit " << MenuConfig::OverlayFpsLimit << '\n';
    f << "SniperCrosshair " << MenuConfig::SniperCrosshair << '\n';
    WriteColor(f, "SniperCrosshairColor", MenuConfig::SniperCrosshairColor);
    f << "SniperCrosshairSize " << MenuConfig::SniperCrosshairSize << '\n';
    f << "SniperCrosshairGap " << MenuConfig::SniperCrosshairGap << '\n';
    f << "SniperCrosshairThickness " << MenuConfig::SniperCrosshairThickness << '\n';
    f << "SniperCrosshairOpacity " << MenuConfig::SniperCrosshairOpacity << '\n';
    f << "SniperCrosshairCenterDot " << MenuConfig::SniperCrosshairCenterDot << '\n';
    f << "SniperCrosshairDotSize " << MenuConfig::SniperCrosshairDotSize << '\n';
    f << "ESPVisibilityMode " << MenuConfig::ESPVisibilityMode << '\n';
    f << "MaxPlayers " << MenuConfig::MaxPlayers << '\n';
    return true;
}

bool MyConfigSaver::LoadConfig(const std::string& filename)
{
    std::ifstream f(FullPath(filename));
    if (!f)
        return false;

    std::string line;
    while (std::getline(f, line))
    {
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key.empty() || key == "version")
            continue;

        auto readBool = [&](bool& v) { std::string s; if (ss >> s) v = ParseBool(s); };
        auto readBind = [&](Keybind::Bind& b) {
            int mode = 0;
            if (ss >> b.key >> mode) {
                mode = std::clamp(mode, 0, 2);
                b.mode = static_cast<Keybind::Mode>(mode);
                b.ResetRuntime();
            }
        };
        auto readColor = [&](ImColor& c) { ss >> c.Value.x >> c.Value.y >> c.Value.z >> c.Value.w; };

        if (key == "ESP") readBool(MenuConfig::ESP);
        else if (key == "TeamCheck") readBool(MenuConfig::TeamCheck);
        else if (key == "ShowBoxESP") readBool(MenuConfig::ShowBoxESP);
        else if (key == "ShowBoneESP") readBool(MenuConfig::ShowBoneESP);
        else if (key == "ShowHealthBar") readBool(MenuConfig::ShowHealthBar);
        else if (key == "ShowWeaponESP") readBool(MenuConfig::ShowWeaponESP);
        else if (key == "ShowDistance") readBool(MenuConfig::ShowDistance);
        else if (key == "ShowPlayerName") readBool(MenuConfig::ShowPlayerName);
        else if (key == "BoxColor") readColor(MenuConfig::BoxColor);
        else if (key == "BoneColor") readColor(MenuConfig::BoneColor);
        else if (key == "TextColor") readColor(MenuConfig::TextColor);
        else if (key == "AimBot") readBool(MenuConfig::AimBot);
        else if (key == "AimKey") readBind(MenuConfig::AimKey);
        else if (key == "AimFov") ss >> MenuConfig::AimFov;
        else if (key == "AimSmooth") ss >> MenuConfig::AimSmooth;
        else if (key == "AimPosition") ss >> MenuConfig::AimPosition;
        else if (key == "AimSpottedOnly") readBool(MenuConfig::AimSpottedOnly);
        else if (key == "ShowAimFovRange") readBool(MenuConfig::ShowAimFovRange);
        else if (key == "AimFovRangeColor") readColor(MenuConfig::AimFovRangeColor);
        else if (key == "RCS") readBool(MenuConfig::RCS);
        else if (key == "RCSStartBullet") ss >> MenuConfig::RCSStartBullet;
        else if (key == "RCSSmooth") ss >> MenuConfig::RCSSmooth;
        else if (key == "RCSPitch") ss >> MenuConfig::RCSPitch;
        else if (key == "RCSYaw") ss >> MenuConfig::RCSYaw;
        else if (key == "RCSWeaponAware") readBool(MenuConfig::RCSWeaponAware);
        else if (key == "TriggerBot") readBool(MenuConfig::TriggerBot);
        else if (key == "TriggerKey") readBind(MenuConfig::TriggerKey);
        else if (key == "TriggerDelay") ss >> MenuConfig::TriggerDelay;
        else if (key == "TriggerHitchance") ss >> MenuConfig::TriggerHitchance;
        else if (key == "TriggerSnipersScopedOnly") readBool(MenuConfig::TriggerSnipersScopedOnly);
        else if (key == "TriggerHitboxHead") readBool(MenuConfig::TriggerHitboxes[0]);
        else if (key == "TriggerHitboxNeck") readBool(MenuConfig::TriggerHitboxes[1]);
        else if (key == "TriggerHitboxChest") readBool(MenuConfig::TriggerHitboxes[2]);
        else if (key == "TriggerHitboxStomach") readBool(MenuConfig::TriggerHitboxes[3]);
        else if (key == "TriggerHitboxPelvis") readBool(MenuConfig::TriggerHitboxes[4]);
        else if (key == "BunnyHop") readBool(MenuConfig::BunnyHop);
        else if (key == "BunnyHopKey") readBind(MenuConfig::BunnyHopKey);
        else if (key == "MenuKey") readBind(MenuConfig::MenuKey);
        else if (key == "PanicKey") readBind(MenuConfig::PanicKey);
        else if (key == "MenuAccentColor") readColor(MenuConfig::MenuAccentColor);
        else if (key == "ShowStatusCounter") readBool(MenuConfig::ShowStatusCounter);
        else if (key == "StatusShowBrand") readBool(MenuConfig::StatusShowBrand);
        else if (key == "StatusShowUser") readBool(MenuConfig::StatusShowUser);
        else if (key == "StatusShowFps") readBool(MenuConfig::StatusShowFps);
        else if (key == "StatusShowPing") readBool(MenuConfig::StatusShowPing);
        else if (key == "ShowSpectatorList") readBool(MenuConfig::ShowSpectatorList);
        else if (key == "OverlayFpsLimit") ss >> MenuConfig::OverlayFpsLimit;
        else if (key == "SniperCrosshair") readBool(MenuConfig::SniperCrosshair);
        else if (key == "SniperCrosshairColor") readColor(MenuConfig::SniperCrosshairColor);
        else if (key == "SniperCrosshairSize") ss >> MenuConfig::SniperCrosshairSize;
        else if (key == "SniperCrosshairGap") ss >> MenuConfig::SniperCrosshairGap;
        else if (key == "SniperCrosshairThickness") ss >> MenuConfig::SniperCrosshairThickness;
        else if (key == "SniperCrosshairOpacity") ss >> MenuConfig::SniperCrosshairOpacity;
        else if (key == "SniperCrosshairCenterDot") readBool(MenuConfig::SniperCrosshairCenterDot);
        else if (key == "SniperCrosshairDotSize") ss >> MenuConfig::SniperCrosshairDotSize;
        else if (key == "ESPVisibilityMode") ss >> MenuConfig::ESPVisibilityMode;
        else if (key == "MaxPlayers") ss >> MenuConfig::MaxPlayers;
    }

    MenuConfig::AimFov = std::clamp(MenuConfig::AimFov, 0.25f, 30.f);
    MenuConfig::AimSmooth = std::clamp(MenuConfig::AimSmooth, 1.f, 30.f);
    MenuConfig::AimPosition = std::clamp(MenuConfig::AimPosition, 0, 2);
    MenuConfig::RCSStartBullet = std::clamp(MenuConfig::RCSStartBullet, 1, 10);
    MenuConfig::RCSSmooth = std::clamp(MenuConfig::RCSSmooth, 1.f, 12.f);
    MenuConfig::TriggerDelay = std::clamp(MenuConfig::TriggerDelay, 0, 1000);
    MenuConfig::TriggerHitchance = std::clamp(MenuConfig::TriggerHitchance, 0, 100);
    MenuConfig::MaxPlayers = std::clamp(MenuConfig::MaxPlayers, 1, 64);
    MenuConfig::OverlayFpsLimit = std::clamp(MenuConfig::OverlayFpsLimit, 30, 400);
    MenuConfig::SniperCrosshairSize = std::clamp(MenuConfig::SniperCrosshairSize, 1.f, 40.f);
    MenuConfig::SniperCrosshairGap = std::clamp(MenuConfig::SniperCrosshairGap, 0.f, 30.f);
    MenuConfig::SniperCrosshairThickness = std::clamp(MenuConfig::SniperCrosshairThickness, 0.5f, 8.f);
    MenuConfig::SniperCrosshairOpacity = std::clamp(MenuConfig::SniperCrosshairOpacity, 0.f, 1.f);
    MenuConfig::SniperCrosshairDotSize = std::clamp(MenuConfig::SniperCrosshairDotSize, 1.f, 12.f);
    MenuConfig::ESPVisibilityMode = std::clamp(MenuConfig::ESPVisibilityMode, 0, 2);
    return true;
}
