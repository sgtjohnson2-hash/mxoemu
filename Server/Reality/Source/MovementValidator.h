#ifndef MXOEMU_MOVEMENT_VALIDATOR_H
#define MXOEMU_MOVEMENT_VALIDATOR_H

#include "Common.h"
#include "Singleton.h"
#include <atomic>

class PlayerObject;

enum MovementValidationResult
{
    MOVE_VALID                 = 0,
    MOVE_REJECT_SPEEDHACK      = 1,
    MOVE_REJECT_NOCLIP_COLLIDER = 2,
    MOVE_REJECT_OUT_OF_BOUNDS  = 3
};

class MovementValidator : public Singleton<MovementValidator>
{
public:
    MovementValidator();
    ~MovementValidator();

    MovementValidationResult ValidateMovement(
        PlayerObject* player,
        float startX, float startY, float startZ,
        float targetX, float targetY, float targetZ,
        uint32 deltaMs,
        float& outRollbackX, float& outRollbackY, float& outRollbackZ);

    bool CheckStaticCollision(float x, float y, float z, float radius = 50.0f) const;
    bool CheckTrajectoryLineOfSight(float startX, float startY, float startZ, float targetX, float targetY, float targetZ) const;

    void GetViolationStats(uint64& speedViolations, uint64& collisionViolations) const;

private:
    static constexpr float MAX_SPEED_UNITS_PER_SEC = 1600.0f; // Wire-fu jump upper limit
    static constexpr float COLLISION_TOLERANCE_RADIUS = 40.0f;

    mutable std::atomic<uint64> m_speedViolations{0};
    mutable std::atomic<uint64> m_collisionViolations{0};
};

#define sMovementValidator MovementValidator::getSingleton()

#endif // MXOEMU_MOVEMENT_VALIDATOR_H
