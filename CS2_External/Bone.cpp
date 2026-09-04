#include "Bone.h"

bool CBone::UpdateAllBoneData(const std::uintptr_t& entityPawnAddress)
{
    BonePosList.clear();
    if (!entityPawnAddress)
        return false;
    EntityPawnAddress = entityPawnAddress;

    std::uintptr_t sceneNode = 0;
    std::uintptr_t boneArrayAddress = 0;
    if (!ProcessMgr.ReadMemory(entityPawnAddress + Offset::Pawn.GameSceneNode, sceneNode) || !sceneNode)
        return false;
    if (!ProcessMgr.ReadMemory(sceneNode + Offset::Pawn.BoneArray, boneArrayAddress) || !boneArrayAddress)
        return false;

    constexpr std::size_t kBoneCount = 24;
    BoneJointData bones[kBoneCount]{};
    if (!ProcessMgr.ReadBuffer(boneArrayAddress, bones, sizeof(bones)))
        return false;

    BonePosList.reserve(kBoneCount);
    for (const auto& bone : bones)
    {
        Vec2 screen{};
        const bool visible = gGame.View.WorldToScreen(bone.Pos, screen);
        BonePosList.push_back({ bone.Pos, screen, visible });
    }
    return BonePosList.size() == kBoneCount;
}
