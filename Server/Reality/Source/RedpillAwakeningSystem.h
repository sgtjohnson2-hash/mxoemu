#ifndef MXOEMU_REDPILL_AWAKENING_SYSTEM_H
#define MXOEMU_REDPILL_AWAKENING_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <memory>

enum MatrixAnomalyType
{
    ANOMALY_DEJA_VU_BLACK_CAT = 0,
    ANOMALY_INVERTED_RAINDROPS = 1,
    ANOMALY_WIREFRAME_FLICKER = 2,
    ANOMALY_BENT_SPOON = 3
};

enum PotentialEscortState
{
    POTENTIAL_UNAWARE = 0,
    POTENTIAL_GLITCH_OBSERVED = 1,
    POTENTIAL_AWAKENED = 2,
    POTENTIAL_ESCORT_ACTIVE = 3,
    POTENTIAL_EXTRACTED_SAFE = 4,
    POTENTIAL_ELIMINATED = 5
};

struct AwakeningVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    AwakeningVector3() = default;
    AwakeningVector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    AwakeningVector3 operator+(const AwakeningVector3& o) const { return AwakeningVector3(x + o.x, y + o.y, z + o.z); }
    AwakeningVector3 operator-(const AwakeningVector3& o) const { return AwakeningVector3(x - o.x, y - o.y, z - o.z); }
    AwakeningVector3 operator*(float s) const { return AwakeningVector3(x * s, y * s, z * s); }
};

struct CivilianPotential
{
    uint32 potentialId{1};
    uint32 npcGoId{5001};
    std::string civilianName{"Thomas Anderson"};
    uint32 districtId{1};
    AwakeningVector3 position{99640.0f, 500.0f, 8350.0f};

    float systemDisbeliefPercent{0.0f}; // 0..100%
    PotentialEscortState state{POTENTIAL_UNAWARE};
    uint32 assignedEscortPlayerGoId{0};
    uint32 targetHardlineId{1};

    float glitchProximityRadius{600.0f};
    uint32 dejaVuCatOccurrences{0};
    bool isAgentTargeted{false};
};

struct MobilAveSmugglingContract
{
    uint32 contractId{1};
    std::string contrabandCodeName{"Subterranean Gate Cipher"};
    uint32 rewardInfoCurrency{15000};
    bool isCompleted{false};
    std::string destinationDistrict{"Slums"};
};

class RedpillAwakeningSystem : public Singleton<RedpillAwakeningSystem>
{
public:
    RedpillAwakeningSystem();
    ~RedpillAwakeningSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Civilian Awakening & Glitches
    uint32 RegisterCivilianPotential(uint32 npcGoId, const std::string& name, uint32 districtId, const AwakeningVector3& pos);
    bool TriggerMatrixAnomaly(uint32 potentialId, MatrixAnomalyType type);
    void ExposeCivilianToCombatEvent(float posX, float posZ, float intensity);

    // Escort & Hardline Extraction
    bool BeginEscort(uint32 potentialId, uint32 playerGoId, uint32 targetHardlineId);
    bool CheckHardlineExtraction(uint32 potentialId, float hardlineX, float hardlineZ);

    // Mobil Ave Purgatory Station & Smuggling
    uint32 CreateSmugglingContract(const std::string& codeName, uint32 rewardInfo);
    bool CompleteSmugglingContract(uint32 contractId);

    // Telemetry & Getters
    const CivilianPotential* GetPotential(uint32 potentialId) const;
    size_t GetPotentialCount() const;
    size_t GetAwakenedCount() const;
    size_t GetTotalExtracted() const { return m_totalExtracted; }

private:
    mutable std::recursive_mutex m_awakeningMutex;
    std::map<uint32, CivilianPotential> m_potentials;
    std::vector<MobilAveSmugglingContract> m_contracts;

    uint32 m_nextPotentialId{1};
    uint32 m_nextContractId{1};
    uint32 m_totalExtracted{0};
    float m_simTimeSec{0.0f};
};

#define sRedpillAwakeningSystem RedpillAwakeningSystem::getSingleton()

#endif // MXOEMU_REDPILL_AWAKENING_SYSTEM_H
