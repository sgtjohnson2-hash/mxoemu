#ifndef MXOSIM_BOTCLIENT_H
#define MXOSIM_BOTCLIENT_H

#include "GameClient.h"
#include <vector>
#include <utility>
#include <algorithm>
#include "BehaviorTree.h"
#include "AI/ActiveInferenceSolver.h"
#include "AI/QTable.h"
#include "AI/BotPersonality.h"
#include "AI/TheoryOfMind.h"
#include "AI/MemoryStreamCuller.h"
#include "AI/GOAPPlanner.h"
class BehaviorNode;

struct BotVector2D {
    float x;
    float z;
    BotVector2D() : x(0), z(0) {}
    BotVector2D(float _x, float _z) : x(_x), z(_z) {}
};

// Faction is now in Common.h
enum class ExecutionLOD {
    ACTIVE_VIEWPORT = 0,   // < 50m: 4Hz Tick (250ms)
    APPROACH_AREA = 1,     // 50m - 200m: 1Hz Tick (1000ms)
    BACKGROUND_AREA = 2    // > 200m: 0Hz Tick (skip logic)
};

//exception-safe object lookup shared by the bot AI (defined in BotClient.cpp)
class PlayerObject* BotGetPlayer(uint32 goId);

class BotClient : public GameClient
{
public:
    BotClient(uint64 charUID);
    virtual ~BotClient();

    virtual bool isBot() const override { return true; }

    virtual void FlushQueue(bool alsoResend = false);
    virtual void CheckAndResend();

    float m_deltaSeconds = 0.033f;
    float GetDeltaSeconds() const { return m_deltaSeconds; }
    void UpdateBotAI(float deltaSeconds);
    void RoamAndSwarm(float deltaSeconds = -1.0f);
    void AttackTarget(uint32 targetGoId);
    void MoveTo(float x, float y, float z);

    void Say(const std::string& msg);
    void Emote(uint32 emoteId);

    void SetBehaviorTree(std::shared_ptr<class BehaviorNode> tree) { m_behaviorTree = tree; }
    void SetFaction(mxoFaction faction) { m_faction = faction; }
    mxoFaction GetFaction() const { return m_faction; }

    bool isAgent() const { return m_isAgent; }
    void setAgent(bool val) { m_isAgent = val; }

    bool IsPanicking() const;
    void SetPanicking(bool panic);
    void triggerPanic(uint32 sourceGoId);

    float GetFearLevel() const;
    void SetFearLevel(float f);
    void AddFear(float amount);
    uint8 GetCivilianTier() const;

    uint32 GetLastDistressCallTime() const;
    void SetLastDistressCallTime(uint32 t);
    uint32 GetLastLookAroundTime() const;
    void SetLastLookAroundTime(uint32 t);
    uint32 GetLastWhisperTime() const;
    void SetLastWhisperTime(uint32 t);

    uint32 GetTargetGoId() const { return m_targetGoId; }
    void SetTargetGoId(uint32 id) { m_targetGoId = id; }
    bool IsInCombat() const { return m_targetGoId != 0; }
    uint32 GetNextActionTime() const { return m_nextActionTime; }
    void SetNextActionTime(uint32 time) { m_nextActionTime = time; }
    void SetSpawnLocation(float x, float y, float z) { m_spawnX = x; m_spawnY = y; m_spawnZ = z; }
    float GetSpawnX() const { return m_spawnX; }
    float GetSpawnY() const { return m_spawnY; }
    float GetSpawnZ() const { return m_spawnZ; }

    ExecutionLOD GetLOD() const { return m_currentLOD; }
    void SetLOD(ExecutionLOD lod) { m_currentLOD = lod; }
    uint32 GetLastLodTick() const { return m_lastLodTickMS; }
    void SetLastLodTick(uint32 tick) { m_lastLodTickMS = tick; }

    static QTable s_qTable;

    const BotPersonality& GetPersonality() const { return *m_personality; }
    void SetPersonality(const BotPersonality* p) { m_personality = p; }
    ActiveInferenceSolver& GetActiveInferenceSolver() { return m_activeInference; }
    const SomaticMarkers& GetSomaticMarkers() const { return m_somatic; }
    TheoryOfMindSolver& GetTheoryOfMindSolver() { return m_tomSolver; }
    MemoryStreamCuller& GetMemoryStreamCuller() { return m_memoryStream; }

    uint32 GetCrewLeaderId() const { return m_crewLeaderId; }
    void SetCrewLeaderId(uint32 id) { m_crewLeaderId = id; }
    uint32 GetCrewId() const { return m_crewId; }
    void SetCrewId(uint32 id) { m_crewId = id; }

    uint32 GetPossessedBy() const { return m_possessedBy; }
    void SetPossessedBy(uint32 goId) { m_possessedBy = goId; }

    uint32 GetLastGossipTime() const { return m_lastGossipTime; }
    void SetLastGossipTime(uint32 t) { m_lastGossipTime = t; }

private:
    BotVector2D CalculateBoidsVelocity(class PlayerObject* me);

    uint32 m_targetGoId;
    uint32 m_stateTimer;
    uint32 m_nextActionTime;
    bool m_isPanicking = false;
    float m_fearLevel = 0.0f;
    uint32 m_lastGossipTime = 0;
    uint32 m_lastDistressCallTime = 0;
    uint32 m_lastLookAroundTime = 0;
    uint32 m_lastWhisperTime = 0;
    
    std::shared_ptr<BehaviorNode> m_btRoot;
    GOAPPlanner m_goapPlanner;
    float m_spawnX;
    float m_spawnY;
    float m_spawnZ;
    mxoFaction m_faction;
    uint32 m_crewLeaderId;
    uint32 m_crewId;
    std::shared_ptr<class BehaviorNode> m_behaviorTree;
    
    ActiveInferenceSolver m_activeInference = ActiveInferenceSolver(0.1f, 0.9f);
    SomaticMarkers m_somatic;
    const BotPersonality* m_personality;
    TheoryOfMindSolver m_tomSolver;

    MemoryStreamCuller m_memoryStream;
    std::vector<std::pair<float, float>> m_pathWaypoints;
    int m_currentWaypointIndex;

    ExecutionLOD m_currentLOD;
    uint32 m_lastLodTickMS;
    
    uint32 m_possessedBy;
    bool m_isAgent = false;
};

#endif
