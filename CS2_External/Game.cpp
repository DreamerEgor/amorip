#include "Game.h"
#include <algorithm>
#include <cmath>

bool CGame::InitAddress()
{
    Address.ClientDLL = reinterpret_cast<std::uintptr_t>(ProcessMgr.GetProcessModuleHandle("client.dll"));
    if (!Address.ClientDLL)
        return false;

    Address.EntityList = Address.ClientDLL + Offset::EntityList;
    Address.Matrix = Address.ClientDLL + Offset::Matrix;
    Address.ViewAngle = Address.ClientDLL + Offset::ViewAngle;
    Address.LocalController = Address.ClientDLL + Offset::LocalPlayerController;
    Address.LocalPawn = Address.ClientDLL + Offset::LocalPlayerPawn;
    Address.Sensitivity = Address.ClientDLL + Offset::Sensitivity;
    EntityListOwner = 0;
    EntityStride = 0;
    return true;
}

bool CGame::ReadLocalController(std::uintptr_t& out) const
{
    out = 0;
    return ProcessMgr.ReadMemory(Address.LocalController, out) && out != 0;
}

bool CGame::ReadLocalPawn(std::uintptr_t& out) const
{
    out = 0;
    return ProcessMgr.ReadMemory(Address.LocalPawn, out) && out != 0;
}

bool CGame::ReadViewAngle(Vec2& out) const
{
    return ProcessMgr.ReadMemory(Address.ViewAngle, out) && std::isfinite(out.x) && std::isfinite(out.y);
}

float CGame::ReadSensitivity() const
{
    std::uintptr_t sensitivityObject = 0;
    float value = 0.f;
    if (ProcessMgr.ReadMemory(Address.Sensitivity, sensitivityObject) && sensitivityObject &&
        ProcessMgr.ReadMemory(sensitivityObject + Offset::SensitivityValue, value) &&
        std::isfinite(value) && value > 0.01f && value < 100.f)
        return value;

    // Safe fallback when the convar pointer changes temporarily during map loading.
    return 2.5f;
}

bool CGame::CalibrateEntityResolver(std::uintptr_t knownLocalController) const
{
    if (!knownLocalController)
        return false;

    // dwEntityList points at an entity-system root pointer on current builds.
    // Keep a direct-address fallback as well because community/public layouts have
    // differed here over time. The slot stride has likewise appeared as both 0x70
    // and 0x78, so identify the correct one by finding the known local controller.
    std::uintptr_t indirectOwner = 0;
    ProcessMgr.ReadMemory(Address.EntityList, indirectOwner);

    const std::uintptr_t owners[] = { indirectOwner, Address.EntityList };
    constexpr std::size_t strides[] = { 0x70, 0x78 };

    for (const auto owner : owners)
    {
        if (!owner)
            continue;

        std::uintptr_t page0 = 0;
        if (!ProcessMgr.ReadMemory(owner + 0x10, page0) || !page0)
            continue;

        for (const auto stride : strides)
        {
            // Player controllers normally occupy the first slots, but scan a little
            // farther so the calibration remains robust in local/bot sessions.
            for (std::uint32_t index = 1; index <= 128; ++index)
            {
                std::uintptr_t candidate = 0;
                if (!ProcessMgr.ReadMemory(page0 + stride * index, candidate))
                    continue;
                if (candidate == knownLocalController)
                {
                    EntityListOwner = owner;
                    EntityStride = stride;
                    return true;
                }
            }
        }
    }

    EntityListOwner = 0;
    EntityStride = 0;
    return false;
}

std::uintptr_t CGame::ResolveEntity(std::uint32_t handleOrIndex) const
{
    const std::uint32_t index = handleOrIndex & 0x7FFFu;
    if (!index || !EntityListOwner || !EntityStride)
        return 0;

    std::uintptr_t page = 0;
    const std::uintptr_t pageAddress = EntityListOwner + 0x10 + 0x8 * (index >> 9);
    if (!ProcessMgr.ReadMemory(pageAddress, page) || !page)
        return 0;

    std::uintptr_t entity = 0;
    if (!ProcessMgr.ReadMemory(page + EntityStride * (index & 0x1FFu), entity))
        return 0;
    return entity;
}
