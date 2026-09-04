#pragma once
#include <cstdint>
#include <string>

namespace Offset
{
    inline std::uintptr_t EntityList = 0;
    inline std::uintptr_t Matrix = 0;
    inline std::uintptr_t ViewAngle = 0;
    inline std::uintptr_t LocalPlayerController = 0;
    inline std::uintptr_t LocalPlayerPawn = 0;
    inline std::uintptr_t Sensitivity = 0;
    inline std::uintptr_t SensitivityValue = 0;

    struct EntityOffsets
    {
        std::uintptr_t Health = 0;
        std::uintptr_t TeamID = 0;
        std::uintptr_t PlayerPawn = 0;
        std::uintptr_t iszPlayerName = 0;
        std::uintptr_t Ping = 0;
        std::uintptr_t ObserverPawn = 0;
    };
    inline EntityOffsets Entity;

    struct PawnOffsets
    {
        std::uintptr_t Pos = 0;
        std::uintptr_t ViewOffset = 0;
        std::uintptr_t GameSceneNode = 0;
        std::uintptr_t BoneArray = 0;
        std::uintptr_t Dormant = 0;
        std::uintptr_t SceneOrigin = 0;
        std::uintptr_t pWeaponServices = 0;
        std::uintptr_t pObserverServices = 0;
        std::uintptr_t pAimPunchServices = 0;
        std::uintptr_t AimPunchDirectAngle = 0;
        std::uintptr_t AimPunchBaseAngle = 0;
        std::uintptr_t AimPunchUnpredictableAngle = 0;
        std::uintptr_t iShotsFired = 0;
        std::uintptr_t iIDEntIndex = 0;
        std::uintptr_t bSpottedByMask = 0;
        std::uintptr_t bIsScoped = 0;
        std::uintptr_t OnGroundLastTick = 0;
        std::uintptr_t Flags = 0;
    };
    inline PawnOffsets Pawn;

    struct ObserverOffsets
    {
        std::uintptr_t hObserverTarget = 0;
        std::uintptr_t iObserverMode = 0;
    };
    inline ObserverOffsets Observer;

    struct WeaponOffsets
    {
        std::uintptr_t hActiveWeapon = 0;
        std::uintptr_t AttributeManager = 0;
        std::uintptr_t Item = 0;
        std::uintptr_t ItemDefinitionIndex = 0;
    };
    inline WeaponOffsets Weapon;

    bool UpdateOffsets(const std::string& cacheDirectory, std::string* statusMessage = nullptr);
}
