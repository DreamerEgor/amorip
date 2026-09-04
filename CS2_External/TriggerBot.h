#pragma once
#include "Game.h"
#include "Entity.h"
#include <vector>

namespace TriggerBot
{
    void Run(const CEntity& localEntity, const std::vector<CEntity>& entities);
    void Reset();
}
