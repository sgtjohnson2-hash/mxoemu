#include "CrewSystem.h"
#include "PlayerObject.h"
#include "Log.h"
#include "Log.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"

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
    
    // Save to database
    PreparedStatement stmt("INSERT INTO `crews` (`id`, `name`, `faction`, `leader_goid`) VALUES (?0, ?1, ?2, ?3)");
    stmt.SetUInt32(0, newCrew.crewId);
    stmt.SetString(1, newCrew.name);
    stmt.SetUInt32(2, (uint32)newCrew.faction);
    stmt.SetUInt32(3, leader->getGoId());
    sDatabase.ExecutePrepared(&stmt);
    
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

    // Save to database
    PreparedStatement stmt("INSERT INTO `crew_members` (`crew_id`, `member_goid`, `rank`) VALUES (?0, ?1, ?2)");
    stmt.SetUInt32(0, it->second.crewId);
    stmt.SetUInt32(1, player->getGoId());
    stmt.SetUInt32(2, 1);
    sDatabase.ExecutePrepared(&stmt);

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

    // Remove from database
    PreparedStatement stmt("DELETE FROM `crew_members` WHERE `member_goid` = ?0");
    stmt.SetUInt32(0, player->getGoId());
    sDatabase.ExecutePrepared(&stmt);

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
