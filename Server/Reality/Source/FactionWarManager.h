#ifndef MXOEMU_FACTION_WAR_MANAGER_H
#define MXOEMU_FACTION_WAR_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <map>
#include <vector>
#include <string>

struct ControlNode {
    uint32 id;
    uint32 districtId;
    float x, y, z;
    uint32 controllingFaction; // 0=Machine, 1=Zion, 2=Merovingian
    float captureProgress;
    uint32 capturingFaction;
    bool isContested;
    uint32 lastContestedMs;
};

struct DistrictStatus {
    uint32 districtId;
    std::string name;
    uint32 totalNodes;
    uint32 machineNodes;
    uint32 zionNodes;
    uint32 meroNodes;
    float machineInfluence;
    float zionInfluence;
    float meroInfluence;
    uint32 dominantFaction;
    std::vector<uint32> contestedNodeIds;
};

enum StrikeSquadRole {
    ROLE_OPERATIVE_TANK = 0,
    ROLE_HACKER_SUPPORT = 1,
    ROLE_MARTIAL_ARTIST_FLANK = 2
};

struct SquadMember {
    uint32 goId;
    StrikeSquadRole role;
    bool isAlive;
};

enum SquadTacticalState {
    SQUAD_STATE_MOBILIZING = 0,
    SQUAD_STATE_ADVANCING = 1,
    SQUAD_STATE_ENGAGING = 2,
    SQUAD_STATE_HOLDING = 3,
    SQUAD_STATE_RETREATING = 4
};

struct StrikeSquad {
    uint32 squadId;
    uint32 faction;
    uint32 targetNodeId;
    LocationVector targetPos;
    SquadTacticalState state;
    SquadMember operative;
    SquadMember hacker;
    SquadMember martialArtist;
    uint32 lastCalloutTime;
    uint32 spawnTime;
    bool active;
};

class StrategicFactionCommander {
public:
    StrategicFactionCommander();
    StrategicFactionCommander(uint32 faction, const std::string& name);

    void EvaluateFrontlines(const std::map<uint32, DistrictStatus>& districts, const std::map<uint32, ControlNode>& nodes, uint32 currentMs);
    uint32 GetPreferredTargetNode() const { return m_preferredTargetNodeId; }
    uint32 GetFaction() const { return m_faction; }
    const std::string& GetName() const { return m_name; }
    uint32 GetWarSupply() const { return m_warSupply; }
    void AddWarSupply(uint32 amount) { m_warSupply += amount; }
    void SpendWarSupply(uint32 amount) { if (m_warSupply >= amount) m_warSupply -= amount; else m_warSupply = 0; }

private:
    uint32 m_faction;
    std::string m_name;
    uint32 m_warSupply;
    uint32 m_preferredTargetNodeId;
    uint32 m_lastEvalMs;
};

class FactionWarManager : public Singleton<FactionWarManager> {
public:
    FactionWarManager();
    ~FactionWarManager();

    void initialize();
    void update(uint32 deltaMs);
    
    // Tally a PvP kill
    void registerPvPKill(uint32 killerFaction, uint32 victimFaction);

    void updateBroadcasts(uint32 deltaMs);

    void registerControlNode(uint32 id, float x, float y, float z) { registerControlNode(id, 1, x, y, z); }
    void registerControlNode(uint32 id, uint32 districtId, float x, float y, float z);
    void captureNode(uint32 id, uint32 newFaction);
    uint32 getControllingFaction(uint32 nodeId);
    uint32 getControllingFactionByLocation(float x, float y, float z);
    bool getClosestEnemyNode(uint32 myFaction, float x, float y, float z, float& outX, float& outY, float& outZ);

    // Frontline & District Queries
    const std::map<uint32, DistrictStatus>& GetDistricts() const { return m_districtStatus; }
    const DistrictStatus* GetDistrict(uint32 districtId) const;
    const std::map<uint32, ControlNode>& GetControlNodes() const { return m_controlNodes; }
    const std::vector<StrikeSquad>& GetActiveSquads() const { return m_strikeSquads; }
    
    // Strategic Deployment
    bool DeployStrikeSquad(uint32 faction, uint32 targetNodeId);
    void BroadcastSquadCallout(const StrikeSquad& squad, const std::string& roleStr, const std::string& msg);

    // Phase 8: Dynamic Territory Buffs
    bool HasDistrictDominance(uint32 districtId, uint32 faction) const;
    std::string GetActiveDistrictBuffName(uint32 districtId) const;
    float GetDistrictHealthRegenBonus(uint32 districtId, uint32 faction) const;
    float GetDistrictWireFuBonus(uint32 districtId, uint32 faction) const;
    float GetDistrictInfoGainBonus(uint32 districtId, uint32 faction) const;

private:
    void syncToDatabase();
    void updateControlNodes(uint32 deltaMs);
    void updateDistrictFrontlines(uint32 deltaMs);
    void updateFactionCommanders(uint32 deltaMs);
    void updateStrikeSquads(uint32 deltaMs);

    std::map<uint32, uint32> m_factionScores;
    std::map<uint32, ControlNode> m_controlNodes;
    std::map<uint32, DistrictStatus> m_districtStatus;
    std::vector<StrategicFactionCommander> m_commanders;
    std::vector<StrikeSquad> m_strikeSquads;
    uint32 m_nextSquadId;
    uint32 m_timeSinceLastSync;
    uint32 m_lastStrategicTickMs;
};

#define sFactionWarMgr Singleton<FactionWarManager>::getSingleton()

#endif // MXOEMU_FACTION_WAR_MANAGER_H
