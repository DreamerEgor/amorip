#pragma once
#include <cstdint>
#include <string>

// CS2 weapon definition indices
namespace WEAPONINDEX
{
    constexpr std::uint16_t AK47 = 7;      // AK-47
    constexpr std::uint16_t M4A4 = 16;     // M4A4
    constexpr std::uint16_t M4A1_S = 60;   // M4A1-S
}

namespace WeaponProfiles
{
    struct Profile
    {
        std::uint16_t DefinitionIndex;
        const char* Name;
        bool IsSniperRifle;
        bool IsAR;
        bool IsSMG;
    };

    inline const Profile GetProfile(std::uint16_t weaponIndex)
    {
        switch (weaponIndex)
        {
        case WEAPONINDEX::AK47:
            return { WEAPONINDEX::AK47, "AK-47", false, true, false };
        case WEAPONINDEX::M4A4:
            return { WEAPONINDEX::M4A4, "M4A4", false, true, false };
        case WEAPONINDEX::M4A1_S:
            return { WEAPONINDEX::M4A1_S, "M4A1-S", false, true, false };
        default:
            return { weaponIndex, "unknown", false, false, false };
        }
    }

    inline const char* GetWeaponName(std::uint16_t weaponIndex)
    {
        return GetProfile(weaponIndex).Name;
    }
}
