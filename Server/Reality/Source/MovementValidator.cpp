#include "MovementValidator.h"
#include "StaticObjectManager.h"
#include "PlayerObject.h"
#include "Log.h"
#include "BotManager.h"
#include <cmath>

createFileSingleton(MovementValidator);

MovementValidator::MovementValidator()
{
}

MovementValidator::~MovementValidator()
{
}

MovementValidationResult MovementValidator::ValidateMovement(
    PlayerObject* player,
    float startX, float startY, float startZ,
    float targetX, float targetY, float targetZ,
    uint32 deltaMs,
    float& outRollbackX, float& outRollbackY, float& outRollbackZ)
{
    outRollbackX = startX;
    outRollbackY = startY;
    outRollbackZ = startZ;

    if (deltaMs == 0) deltaMs = 33; // Avoid div by zero

    // 0. World Boundary Sanity Check (reject NaN, Inf, and positions outside megacity limits)
    if (std::isnan(targetX) || std::isnan(targetY) || std::isnan(targetZ) ||
        std::isinf(targetX) || std::isinf(targetY) || std::isinf(targetZ) ||
        targetX < -500000.0f || targetX > 500000.0f ||
        targetZ < -500000.0f || targetZ > 500000.0f ||
        targetY < -15000.0f  || targetY > 60000.0f)
    {
        if (player)
        {
            WARNING_LOG(format("Anti-Cheat: Out-of-bounds movement for %1% to (%2%, %3%, %4%)")
                        % player->getHandle() % targetX % targetY % targetZ);
            sBotMgr.LogCombat("Movement rejected: Target position is outside simulation matrix boundaries.");
        }
        return MOVE_REJECT_OUT_OF_BOUNDS;
    }

    float dx = targetX - startX;
    float dy = targetY - startY;
    float dz = targetZ - startZ;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    float speed = (dist * 1000.0f) / static_cast<float>(deltaMs);

    // 1. Check maximum legal speed limits (SpeedHack anti-cheat)
    if (speed > MAX_SPEED_UNITS_PER_SEC)
    {
        m_speedViolations.fetch_add(1, std::memory_order_relaxed);
        if (player)
        {
            WARNING_LOG(format("Anti-Cheat: Speedhack detected for %1% (Speed: %2% units/s, Max: %3%)")
                        % player->getHandle() % speed % MAX_SPEED_UNITS_PER_SEC);
            sBotMgr.LogCombat("Movement rejected: Speed exceeds simulation limits.");
        }
        return MOVE_REJECT_SPEEDHACK;
    }

    // 2. Check Static World Colliders (56,100 Static AABBs)
    if (CheckStaticCollision(targetX, targetY, targetZ, COLLISION_TOLERANCE_RADIUS))
    {
        m_collisionViolations.fetch_add(1, std::memory_order_relaxed);
        if (player)
        {
            DEBUG_LOG(format("Anti-Cheat: Collision penetration at (%1%, %2%, %3%) for %4%")
                      % targetX % targetY % targetZ % player->getHandle());
        }
        return MOVE_REJECT_NOCLIP_COLLIDER;
    }

    // 3. Line of sight trajectory raycast through solid walls
    if (!CheckTrajectoryLineOfSight(startX, startY, startZ, targetX, targetY, targetZ))
    {
        m_collisionViolations.fetch_add(1, std::memory_order_relaxed);
        if (player)
        {
            DEBUG_LOG(format("Anti-Cheat: Wallclip trajectory from (%1%, %2%) to (%3%, %4%) for %5%")
                      % startX % startZ % targetX % targetZ % player->getHandle());
        }
        return MOVE_REJECT_NOCLIP_COLLIDER;
    }

    return MOVE_VALID;
}

bool MovementValidator::CheckStaticCollision(float x, float y, float z, float radius) const
{
    return sStaticObjMgr.CheckCollision(x, y, z, radius);
}

bool MovementValidator::CheckTrajectoryLineOfSight(float startX, float startY, float startZ, float targetX, float targetY, float targetZ) const
{
    return sStaticObjMgr.CheckLineOfSight(startX, startY, startZ, targetX, targetY, targetZ);
}

void MovementValidator::GetViolationStats(uint64& speedViolations, uint64& collisionViolations) const
{
    speedViolations = m_speedViolations.load(std::memory_order_relaxed);
    collisionViolations = m_collisionViolations.load(std::memory_order_relaxed);
}
