#pragma once
#include <vector>
#include <list>
#include <cstdint>
#include "Game.h"

// Current standard CS2 player skeleton indices (Source 2 player rig).
// Bone transforms are read as 0x20-byte entries from the skeleton model state.
enum BONEINDEX : std::uint32_t
{
    origin = 0,
    pelvis = 1,
    spine_0 = 2,
    spine_1 = 3,
    spine_2 = 4,
    neck_0 = 6,
    head = 7,
    arm_upper_L = 9,
    arm_lower_L = 10,
    hand_L = 11,
    arm_upper_R = 13,
    arm_lower_R = 14,
    hand_R = 15,
    leg_upper_L = 17,
    leg_lower_L = 18,
    ankle_L = 19,
    leg_upper_R = 20,
    leg_lower_R = 21,
    ankle_R = 22,
};

struct BoneJointData
{
    Vec3 Pos;
    char pad[0x14];
};

struct BoneJointPos
{
    Vec3 Pos;
    Vec2 ScreenPos;
    bool IsVisible = false;
};

class CBone
{
private:
    std::uintptr_t EntityPawnAddress = 0;
public:
    std::vector<BoneJointPos> BonePosList;
    bool UpdateAllBoneData(const std::uintptr_t& entityPawnAddress);
};

namespace BoneJointList
{
    inline std::list<std::uint32_t> Trunk = { head, neck_0, spine_2, spine_1, spine_0, pelvis };
    inline std::list<std::uint32_t> LeftArm = { neck_0, arm_upper_L, arm_lower_L, hand_L };
    inline std::list<std::uint32_t> RightArm = { neck_0, arm_upper_R, arm_lower_R, hand_R };
    inline std::list<std::uint32_t> LeftLeg = { pelvis, leg_upper_L, leg_lower_L, ankle_L };
    inline std::list<std::uint32_t> RightLeg = { pelvis, leg_upper_R, leg_lower_R, ankle_R };
    inline std::vector<std::list<std::uint32_t>> List = { Trunk, LeftArm, RightArm, LeftLeg, RightLeg };
}
