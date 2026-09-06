#ifndef MXOEMU_COVERSYSTEM_H
#define MXOEMU_COVERSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <vector>
#include <mutex>

enum CoverType : uint8
{
    COVER_NONE = 0,
    COVER_LOW = 1,  // Crouch behind barrier
    COVER_HIGH = 2  // Lean around pillar / corner
};

struct CoverPoint
{
    float x;
    float y;
    float z;
    CoverType type;
    float score;
    bool threatOccluded;
    bool hasFlankSight;
};

class CoverSystem : public Singleton<CoverSystem>
{
public:
    CoverSystem();
    ~CoverSystem();

    void Initialize();

    // Procedural cover point extraction & scoring
    bool FindBestCover(float botX, float botZ, float threatX, float threatZ,
                       float searchRadius, CoverPoint& outCoverPoint, float botY = 95.0f);

    // Scoring formula: Score = ThreatOcclusion * 2.0 - TravelDistance * 0.5 + LineOfSightToTarget * 1.2
    float ScoreCoverPoint(float candX, float candZ, float botX, float botZ,
                          float threatX, float threatZ, bool& outOccluded, bool& outHasSight);

private:
    mutable std::mutex m_coverMutex;
};

#define sCoverSystem CoverSystem::getSingleton()

#endif // MXOEMU_COVERSYSTEM_H
