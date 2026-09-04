#pragma once
#include <cstdint>
#include <cstddef>
#include "Utils/ProcessManager.hpp"
#include "Offsets.h"
#include "View.hpp"

class CGame
{
private:
    struct AddressBook
    {
        std::uintptr_t ClientDLL = 0;
        std::uintptr_t EntityList = 0;
        std::uintptr_t Matrix = 0;
        std::uintptr_t ViewAngle = 0;
        std::uintptr_t LocalController = 0;
        std::uintptr_t LocalPawn = 0;
        std::uintptr_t Sensitivity = 0;
    } Address;

    // Source 2 has changed the entity-slot stride between builds. Resolve it at
    // runtime from the already-known local controller instead of hardcoding one
    // layout forever. These are mutable because ResolveEntity is logically a read.
    mutable std::uintptr_t EntityListOwner = 0;
    mutable std::size_t EntityStride = 0;

public:
    CView View;

    bool InitAddress();
    std::uintptr_t GetClientDLLAddress() const { return Address.ClientDLL; }
    std::uintptr_t GetEntityListAddress() const { return Address.EntityList; }
    std::uintptr_t GetMatrixAddress() const { return Address.Matrix; }
    std::uintptr_t GetViewAngleAddress() const { return Address.ViewAngle; }

    bool ReadLocalController(std::uintptr_t& out) const;
    bool ReadLocalPawn(std::uintptr_t& out) const;
    bool ReadViewAngle(Vec2& out) const;
    float ReadSensitivity() const;
    bool CalibrateEntityResolver(std::uintptr_t knownLocalController) const;
    std::size_t GetEntityStride() const { return EntityStride; }
    std::uintptr_t ResolveEntity(std::uint32_t handleOrIndex) const;
};

inline CGame gGame;
