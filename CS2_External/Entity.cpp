#include "Entity.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace
{
    std::string WeaponNameFromId(std::uint16_t id)
    {
        switch (id)
        {
        case 1: return "deagle"; case 2: return "dual berettas"; case 3: return "five-seven";
        case 4: return "glock-18"; case 7: return "ak-47"; case 8: return "aug";
        case 9: return "awp"; case 10: return "famas"; case 11: return "g3sg1";
        case 13: return "galil ar"; case 14: return "m249"; case 16: return "m4a4";
        case 17: return "mac-10"; case 19: return "p90"; case 23: return "mp5-sd";
        case 24: return "ump-45"; case 25: return "xm1014"; case 26: return "pp-bizon";
        case 27: return "mag-7"; case 28: return "negev"; case 29: return "sawed-off";
        case 30: return "tec-9"; case 31: return "zeus"; case 32: return "p2000";
        case 33: return "mp7"; case 34: return "mp9"; case 35: return "nova";
        case 36: return "p250"; case 38: return "scar-20"; case 39: return "sg 553";
        case 40: return "ssg 08"; case 60: return "m4a1-s"; case 61: return "usp-s";
        case 63: return "cz75-auto"; case 64: return "r8 revolver";
        default: return "weapon";
        }
    }


    Vec3 ReadCameraPosition(const PlayerPawn& pawn)
    {
        Vec3 viewOffset{};
        if (Offset::Pawn.ViewOffset && ProcessMgr.ReadMemory(pawn.Address + Offset::Pawn.ViewOffset, viewOffset) &&
            std::isfinite(viewOffset.x) && std::isfinite(viewOffset.y) && std::isfinite(viewOffset.z) &&
            std::fabs(viewOffset.x) < 128.f && std::fabs(viewOffset.y) < 128.f && viewOffset.z > 0.f && viewOffset.z < 128.f)
            return pawn.Pos + viewOffset;
        return pawn.Pos + Vec3(0.f, 0.f, 64.f);
    }

    bool ReadPawnPosition(PlayerPawn& pawn)
    {
        Vec3 pos{};
        const bool oldOriginOk = Offset::Pawn.Pos &&
            ProcessMgr.ReadMemory(pawn.Address + Offset::Pawn.Pos, pos) &&
            std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z) &&
            (std::fabs(pos.x) > 0.001f || std::fabs(pos.y) > 0.001f || std::fabs(pos.z) > 0.001f);

        if (oldOriginOk)
        {
            pawn.Pos = pos;
            return true;
        }

        std::uintptr_t sceneNode = 0;
        if (!Offset::Pawn.GameSceneNode || !Offset::Pawn.SceneOrigin ||
            !ProcessMgr.ReadMemory(pawn.Address + Offset::Pawn.GameSceneNode, sceneNode) || !sceneNode)
            return false;

        Vec3 scenePos{};
        if (!ProcessMgr.ReadMemory(sceneNode + Offset::Pawn.SceneOrigin, scenePos) ||
            !std::isfinite(scenePos.x) || !std::isfinite(scenePos.y) || !std::isfinite(scenePos.z))
            return false;

        pawn.Pos = scenePos;
        return true;
    }

    bool ReadWeapon(PlayerPawn& pawn)
    {
        std::uintptr_t weaponServices = 0;
        if (!ProcessMgr.ReadMemory(pawn.Address + Offset::Pawn.pWeaponServices, weaponServices) || !weaponServices)
            return false;

        std::uint32_t weaponHandle = 0;
        if (!ProcessMgr.ReadMemory(weaponServices + Offset::Weapon.hActiveWeapon, weaponHandle) || !weaponHandle)
            return false;

        const std::uintptr_t weapon = gGame.ResolveEntity(weaponHandle);
        if (!weapon)
            return false;

        const std::uintptr_t itemDef = weapon + Offset::Weapon.AttributeManager + Offset::Weapon.Item + Offset::Weapon.ItemDefinitionIndex;
        std::uint16_t id = 0;
        if (!ProcessMgr.ReadMemory(itemDef, id))
            return false;
        pawn.WeaponDefinitionIndex = id;
        pawn.WeaponName = WeaponNameFromId(id);
        return true;
    }
}

bool CEntity::UpdateController(std::uintptr_t playerControllerAddress)
{
    Controller = {};
    if (!playerControllerAddress)
        return false;
    Controller.Address = playerControllerAddress;

    if (!ProcessMgr.ReadMemory(Controller.Address + Offset::Entity.TeamID, Controller.TeamID))
        return false;
    if (!ProcessMgr.ReadMemory(Controller.Address + Offset::Entity.PlayerPawn, Controller.PawnHandle))
        return false;

    char name[128]{};
    if (ProcessMgr.ReadBuffer(Controller.Address + Offset::Entity.iszPlayerName, name, sizeof(name) - 1))
        Controller.PlayerName = name;
    if (Controller.PlayerName.empty())
        Controller.PlayerName = "player";

    Pawn.Address = gGame.ResolveEntity(Controller.PawnHandle);
    return Pawn.Address != 0;
}

bool CEntity::UpdatePawn(std::uintptr_t playerPawnAddress, bool withBones, bool withWeapon, bool withSpotted)
{
    Pawn.Address = playerPawnAddress;
    if (!Pawn.Address)
        return false;

    if (!ProcessMgr.ReadMemory(Pawn.Address + Offset::Entity.Health, Pawn.Health))
        return false;
    if (!ProcessMgr.ReadMemory(Pawn.Address + Offset::Entity.TeamID, Pawn.TeamID))
        return false;
    if (Pawn.Health <= 0 || Pawn.Health > 100)
        return false;

    if (!ReadPawnPosition(Pawn))
        return false;
    Pawn.CameraPos = ReadCameraPosition(Pawn);
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.bIsScoped, Pawn.IsScoped);

    std::uintptr_t sceneNode = 0;
    if (ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.GameSceneNode, sceneNode) && sceneNode)
        ProcessMgr.ReadMemory(sceneNode + Offset::Pawn.Dormant, Pawn.Dormant);
    // Do not make dormancy a hard failure. On some builds the scene-node dormant
    // flag can lag around transitions; health/position validation below is enough
    // to keep the basic external ESP usable.

    if (withSpotted)
        ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.bSpottedByMask, Pawn.SpottedByMask);
    if (withWeapon)
        ReadWeapon(Pawn);
    // Bone data is optional. A stale/temporarily unavailable bone pointer must not
    // suppress box/name/health ESP for the entire player. Skeleton/aim can use
    // fallbacks when the bone array is unavailable.
    if (withBones)
        Pawn.BoneData.UpdateAllBoneData(Pawn.Address);
    return true;
}

bool CEntity::UpdateLocalPawn(std::uintptr_t playerPawnAddress)
{
    Pawn = {};
    Pawn.Address = playerPawnAddress;
    if (!Pawn.Address)
        return false;

    if (!ProcessMgr.ReadMemory(Pawn.Address + Offset::Entity.Health, Pawn.Health))
        return false;
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Entity.TeamID, Pawn.TeamID);
    if (!ReadPawnPosition(Pawn))
        return false;
    Pawn.CameraPos = ReadCameraPosition(Pawn);
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.iShotsFired, Pawn.ShotsFired);
    bool gotAimPunch = false;

    // Some 2026-era dumps exposed m_aimPunchAngle directly on C_CSPlayerPawn.
    // Prefer it when available because it is already the combined punch value.
    if (Offset::Pawn.AimPunchDirectAngle)
    {
        Vec2 directPunch{};
        if (ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.AimPunchDirectAngle, directPunch) &&
            std::isfinite(directPunch.x) && std::isfinite(directPunch.y))
        {
            Pawn.AimPunchAngle = directPunch;
            gotAimPunch = true;
        }
    }

    // Current schema builds expose recoil through CCSPlayer_AimPunchServices.
    // The effective punch is the predictable + unpredictable components.
    if (!gotAimPunch && Offset::Pawn.pAimPunchServices &&
        Offset::Pawn.AimPunchBaseAngle && Offset::Pawn.AimPunchUnpredictableAngle)
    {
        std::uintptr_t aimPunchServices = 0;
        if (ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.pAimPunchServices, aimPunchServices) && aimPunchServices)
        {
            Vec2 predictable{};
            Vec2 unpredictable{};
            const bool gotPredictable = ProcessMgr.ReadMemory(aimPunchServices + Offset::Pawn.AimPunchBaseAngle, predictable);
            const bool gotUnpredictable = ProcessMgr.ReadMemory(aimPunchServices + Offset::Pawn.AimPunchUnpredictableAngle, unpredictable);
            if (gotPredictable && gotUnpredictable &&
                std::isfinite(predictable.x) && std::isfinite(predictable.y) &&
                std::isfinite(unpredictable.x) && std::isfinite(unpredictable.y))
            {
                Pawn.AimPunchAngle = predictable + unpredictable;
                gotAimPunch = true;
            }
        }
    }
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.iIDEntIndex, Pawn.CrosshairEntityIndex);
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.bIsScoped, Pawn.IsScoped);
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.Flags, Pawn.Flags);
    ProcessMgr.ReadMemory(Pawn.Address + Offset::Pawn.OnGroundLastTick, Pawn.OnGroundLastTick);
    ReadWeapon(Pawn);
    return Pawn.Health > 0 && Pawn.Health <= 100;
}

bool PlayerPawn::IsSniperWeapon() const
{
    return WeaponDefinitionIndex == 9 || WeaponDefinitionIndex == 11 ||
        WeaponDefinitionIndex == 38 || WeaponDefinitionIndex == 40;
}
