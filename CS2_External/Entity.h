#pragma once
#include <cstdint>
#include <string>
#include "Game.h"
#include "Bone.h"

class PlayerController
{
public:
    std::uintptr_t Address = 0;
    std::uint8_t TeamID = 0;
    std::uint32_t PawnHandle = 0;
    std::string PlayerName;
    int Ping = -1;
};

class PlayerPawn
{
public:
    std::uintptr_t Address = 0;
    CBone BoneData;
    Vec3 Pos{};
    Vec2 ScreenPos{};
    Vec3 CameraPos{};
    std::string WeaponName = "unknown";
    std::uint16_t WeaponDefinitionIndex = 0;
    int ShotsFired = 0;
    Vec2 AimPunchAngle{};
    int Health = 0;
    std::uint8_t TeamID = 0;
    std::uint32_t CrosshairEntityIndex = 0xFFFFFFFFu;
    std::uint64_t SpottedByMask = 0;
    bool IsScoped = false;
    std::uint32_t Flags = 0;
    bool OnGroundLastTick = false;
    bool Dormant = false;

    bool IsSniperWeapon() const;
};

class CEntity
{
public:
    PlayerController Controller;
    PlayerPawn Pawn;

    bool UpdateController(std::uintptr_t playerControllerAddress);
    bool UpdatePawn(std::uintptr_t playerPawnAddress, bool withBones = true, bool withWeapon = true, bool withSpotted = true);
    bool UpdateLocalPawn(std::uintptr_t playerPawnAddress);
    bool IsAlive() const { return Pawn.Health > 0 && Pawn.Health <= 100; }
    bool IsInScreen() { return gGame.View.WorldToScreen(Pawn.Pos, Pawn.ScreenPos); }
};
