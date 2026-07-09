#include "CrewSystem.h"
#include "PlayerObject.h"
#include "Log.h"

createFileSingleton(CrewSystem);

CrewSystem::CrewSystem() : m_nextCrewId(1)
{
}

CrewSystem::~CrewSystem()
{
}

uint32 CrewSystem::CreateCrew(const std::string& name, FactionType faction, PlayerObject* leader)
{
    if (!leader) return 0;

    Crew newCrew;
    newCrew.crewId = m_nextCrewId++;
    newCrew.name = name;
    newCrew.faction = faction;
    
    CrewMember lm;
    lm.playerGoId = leader->getGoId();
    lm.handle = leader->getHandle();
    lm.rank = 3; // 3 = Captain in MxO
    newCrew.members.push_back(lm);

    m_crews[newCrew.crewId] = newCrew;
    
    // Update player
    leader->setCrewName(name);
    leader->setFactionName(GetFactionName(faction));

    INFO_LOG(format("Crew %1% created by %2%") % name % leader->getHandle());
    return newCrew.crewId;
}

bool CrewSystem::JoinCrew(uint32 crewId, PlayerObject* player)
{
    if (!player) return false;
    auto it = m_crews.find(crewId);
    if (it == m_crews.end()) return false;

    CrewMember m;
    m.playerGoId = player->getGoId();
    m.handle = player->getHandle();
    m.rank = 1; // 1 = Member
    
    it->second.members.push_back(m);
    
    player->setCrewName(it->second.name);
    player->setFactionName(GetFactionName(it->second.faction));

    INFO_LOG(format("%1% joined crew %2%") % player->getHandle() % it->second.name);
    return true;
}

void CrewSystem::LeaveCrew(PlayerObject* player)
{
    if (!player) return;
    
    std::string crewName = player->getCrewName();
    for (auto& pair : m_crews)
    {
        if (pair.second.name == crewName)
        {
            auto& members = pair.second.members;
            for (auto it = members.begin(); it != members.end(); ++it)
            {
                if (it->playerGoId == player->getGoId())
                {
                    members.erase(it);
                    break;
                }
            }
            break;
        }
    }

    player->setCrewName("");
}

std::string CrewSystem::GetFactionName(FactionType faction) const
{
    switch(faction)
    {
        case FactionType::ZION: return "Zion";
        case FactionType::MACHINES: return "Machines";
        case FactionType::MEROVINGIAN: return "Merovingian";
        default: return "";
    }
}
