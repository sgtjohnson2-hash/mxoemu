#ifndef MXOEMU_RVO2_STEERING_H
#define MXOEMU_RVO2_STEERING_H

#include "Common.h"
#include <cmath>
#include <algorithm>
#include <vector>

struct RVOVector2D
{
    float x;
    float z;

    RVOVector2D() : x(0.0f), z(0.0f) {}
    RVOVector2D(float _x, float _z) : x(_x), z(_z) {}

    RVOVector2D operator+(const RVOVector2D& o) const { return RVOVector2D(x + o.x, z + o.z); }
    RVOVector2D operator-(const RVOVector2D& o) const { return RVOVector2D(x - o.x, z - o.z); }
    RVOVector2D operator*(float s) const { return RVOVector2D(x * s, z * s); }
    RVOVector2D operator/(float s) const { return (std::abs(s) > 0.0001f) ? RVOVector2D(x / s, z / s) : RVOVector2D(0, 0); }

    float LengthSq() const { return x * x + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    RVOVector2D Normalized() const {
        float l = Length();
        return (l > 0.0001f) ? (*this / l) : RVOVector2D(0, 0);
    }

    float Dot(const RVOVector2D& o) const { return x * o.x + z * o.z; }
    float Det(const RVOVector2D& o) const { return x * o.z - z * o.x; }
};

struct RVOAgentState
{
    uint32 goId;
    RVOVector2D position;
    RVOVector2D velocity;
    RVOVector2D prefVelocity;
    float radius;
    float maxSpeed;
};

class RVO2Solver
{
public:
    static constexpr float TIME_HORIZON = 2.0f; // 2 seconds collision lookahead

    static RVOVector2D ComputeRVOVelocity(const RVOAgentState& agent,
                                         const std::vector<RVOAgentState>& neighbors)
    {
        if (neighbors.empty()) {
            return agent.prefVelocity;
        }

        RVOVector2D bestVelocity = agent.prefVelocity;
        float bestPenalty = 0.0f;
        bool hasConflict = false;

        for (const auto& other : neighbors) {
            if (other.goId == agent.goId) continue;

            RVOVector2D relPos = other.position - agent.position;
            float distSq = relPos.LengthSq();
            float combinedRadius = agent.radius + other.radius;
            float combRadiusSq = combinedRadius * combinedRadius;

            if (distSq > 16000000.0f) continue; // > 40m ignore

            // Relative velocity
            RVOVector2D relVel = bestVelocity - other.velocity;
            float relSpeedSq = relVel.LengthSq();

            // Check if relative velocity is inside the Velocity Obstacle cone
            float dist = std::sqrt(std::max(0.01f, distSq));
            if (dist < combinedRadius) {
                // Immediate penetration: emergency push apart
                RVOVector2D pushDir = (relPos.LengthSq() > 0.001f) ? (relPos * -1.0f).Normalized() : RVOVector2D(1.0f, 0.0f);
                bestVelocity = pushDir * agent.maxSpeed;
                hasConflict = true;
                break;
            }

            // Closest approach along relative velocity ray
            if (relSpeedSq > 0.0001f) {
                float timeToClosest = relPos.Dot(relVel) / relSpeedSq;
                if (timeToClosest > 0.0f && timeToClosest < TIME_HORIZON) {
                    RVOVector2D relPosClosest = relPos - relVel * timeToClosest;
                    float perpDistSq = relPosClosest.LengthSq();

                    if (perpDistSq < combRadiusSq) {
                        hasConflict = true;
                        // Avoidance normal pointing away from collision center
                        RVOVector2D normal = (perpDistSq > 0.001f) ? (relPosClosest * -1.0f).Normalized() : RVOVector2D(-relVel.z, relVel.x).Normalized();
                        float overlap = combinedRadius - std::sqrt(std::max(0.0f, perpDistSq));
                        RVOVector2D u = normal * (overlap / std::max(0.1f, timeToClosest));

                        // Reciprocal share: agent adapts by half the vector
                        bestVelocity = bestVelocity + u * 0.5f;
                    }
                }
            }
        }

        // Clamp to max speed
        if (bestVelocity.LengthSq() > agent.maxSpeed * agent.maxSpeed) {
            bestVelocity = bestVelocity.Normalized() * agent.maxSpeed;
        }

        return bestVelocity;
    }
};

#endif // MXOEMU_RVO2_STEERING_H
