#include "CoverSystem.h"
#include "StaticObjectManager.h"
#include "NavMeshMgr.h"
#include "Log.h"
#include <cmath>
#include <algorithm>

createFileSingleton(CoverSystem);

CoverSystem::CoverSystem()
{
}

CoverSystem::~CoverSystem()
{
}

void CoverSystem::Initialize()
{
    INFO_LOG("CoverSystem: Dynamic tactical cover generation initialized.");
}

float CoverSystem::ScoreCoverPoint(float candX, float candZ, float botX, float botZ,
                                  float threatX, float threatZ, bool& outOccluded, bool& outHasSight)
{
    // 1. Threat Occlusion: Does cover block line of sight from threat to cover point?
    outOccluded = !sNavMeshMgr.CheckLineOfSight(threatX, threatZ, candX, candZ);
    float threatOcclusion = outOccluded ? 1.0f : 0.0f;

    // 2. Travel Distance from Bot to Cover Point (normalized to meters, 100 units = 1m)
    float travelDistMeters = std::sqrt(std::pow(candX - botX, 2) + std::pow(candZ - botZ, 2)) / 100.0f;

    // 3. Line of sight / peek potential from offset firing position (0.8m peek out)
    float peekX = candX + (threatX - candX) * 0.1f;
    float peekZ = candZ + (threatZ - candZ) * 0.1f;
    outHasSight = sNavMeshMgr.CheckLineOfSight(peekX, peekZ, threatX, threatZ);
    float lineOfSightToTarget = outHasSight ? 1.0f : 0.0f;

    // Roadmap formula: Score = ThreatOcclusion * 2.0 - TravelDistance * 0.5 + LineOfSightToTarget * 1.2
    float score = (threatOcclusion * 2.0f) - (travelDistMeters * 0.05f) + (lineOfSightToTarget * 1.2f);
    return score;
}

bool CoverSystem::FindBestCover(float botX, float botZ, float threatX, float threatZ,
                               float searchRadius, CoverPoint& outCoverPoint)
{
    std::lock_guard<std::mutex> lock(m_coverMutex);

    auto obstacles = sStaticObjMgr.GetObstaclesInRadius(botX, botZ, searchRadius);
    if (obstacles.empty()) {
        return false;
    }

    float bestScore = -9999.0f;
    bool foundCover = false;
    const float clearance = 250.0f; // 2.5m offset

    for (const auto& box : obstacles) {
        float candPts[4][2] = {
            {box.minX - clearance, (box.minZ + box.maxZ) * 0.5f},
            {box.maxX + clearance, (box.minZ + box.maxZ) * 0.5f},
            {(box.minX + box.maxX) * 0.5f, box.minZ - clearance},
            {(box.minX + box.maxX) * 0.5f, box.maxZ + clearance}
        };

        for (int i = 0; i < 4; ++i) {
            float cx = candPts[i][0];
            float cz = candPts[i][1];

            // Verify point itself is not colliding with building geometry
            if (sStaticObjMgr.CheckCollision(cx, 0.0f, cz, 80.0f)) continue;

            bool occluded = false;
            bool hasSight = false;
            float score = ScoreCoverPoint(cx, cz, botX, botZ, threatX, threatZ, occluded, hasSight);

            if (occluded && score > bestScore) {
                bestScore = score;
                outCoverPoint.x = cx;
                outCoverPoint.y = 95.0f;
                outCoverPoint.z = cz;
                outCoverPoint.type = (i % 2 == 0) ? COVER_HIGH : COVER_LOW;
                outCoverPoint.score = score;
                outCoverPoint.threatOccluded = occluded;
                outCoverPoint.hasFlankSight = hasSight;
                foundCover = true;
            }
        }
    }

    return foundCover;
}
