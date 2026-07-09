#ifndef MXOEMU_CREWSYSTEM_H
#define MXOEMU_CREWSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <map>
#include <vector>

class PlayerObject;

enum class FactionType
{
    NONE = 0,
    ZION = 1,
    MACHINES = 2,
    MEROVINGIAN = 3
};

struct CrewMember
{
    uint32 playerGoId;
    std::string handle;
    uint8 rank;
};

struct Crew
{
    uint32 crewId;
    std::string name;
    FactionType faction;
    std::vector<CrewMember> members;
};

class CrewSystem : public Singleton<CrewSystem>
{
public:
    CrewSystem();
    ~CrewSystem();

    uint32 CreateCrew(const std::string& name, FactionType faction, PlayerObject* leader);
    bool JoinCrew(uint32 crewId, PlayerObject* player);
    void LeaveCrew(PlayerObject* player);
    
    std::string GetFactionName(FactionType faction) const;

private:
    std::map<uint32, Crew> m_crews;
    uint32 m_nextCrewId;
};

#define sCrewSys CrewSystem::getSingleton()

#endif // MXOEMU_CREWSYSTEM_H
