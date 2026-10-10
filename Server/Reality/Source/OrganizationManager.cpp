#include "OrganizationManager.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"
#include "Log.h"
#include <algorithm>

void OrganizationManager::loadFromDB() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_orgs.clear();
    
    PreparedStatement stmt("SELECT `id`, `name`, `leader_goid` FROM `crews`");
    scoped_ptr<QueryResult> res(sDatabase.QueryPrepared(&stmt));
    if (res) {
        do {
            Field* fields = res->Fetch();
            uint32 id = fields[0].GetUInt32();
            std::string name = fields[1].GetString();
            uint64 leader = fields[2].GetUInt32();

            Organization org;
            org.id = id;
            org.name = name;
            org.leaderId = leader;

            // Load members
            PreparedStatement mStmt("SELECT `member_goid` FROM `crew_members` WHERE `crew_id` = ?0");
            mStmt.SetUInt32(0, id);
            scoped_ptr<QueryResult> mRes(sDatabase.QueryPrepared(&mStmt));
            if (mRes) {
                do {
                    Field* mFields = mRes->Fetch();
                    org.members.push_back(mFields[0].GetUInt32());
                } while (mRes->NextRow());
            } else {
                org.members.push_back(leader);
            }

            m_orgs[id] = org;
            if (id >= m_nextOrgId) {
                m_nextOrgId = id + 1;
            }
        } while (res->NextRow());
        INFO_LOG(format("OrganizationManager: Loaded %1% crews from database.") % m_orgs.size());
    }
}

uint32 OrganizationManager::createOrganization(const std::string& name, uint64 leaderId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    uint32 newId = m_nextOrgId++;
    Organization org;
    org.id = newId;
    org.name = name;
    org.leaderId = leaderId;
    org.members.push_back(leaderId);
    m_orgs[newId] = org;

    // Database persistence
    PreparedStatement stmt("INSERT INTO `crews` (`id`, `name`, `faction`, `leader_goid`) VALUES (?0, ?1, 0, ?2) ON DUPLICATE KEY UPDATE `name`=?1, `leader_goid`=?2");
    stmt.SetUInt32(0, newId);
    stmt.SetString(1, name);
    stmt.SetUInt32(2, (uint32)leaderId);
    sDatabase.ExecutePrepared(&stmt);

    PreparedStatement mStmt("INSERT INTO `crew_members` (`crew_id`, `member_goid`, `rank`) VALUES (?0, ?1, 1) ON DUPLICATE KEY UPDATE `rank`=1");
    mStmt.SetUInt32(0, newId);
    mStmt.SetUInt32(1, (uint32)leaderId);
    sDatabase.ExecutePrepared(&mStmt);

    return newId;
}

bool OrganizationManager::joinOrganization(uint32 orgId, uint64 memberId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_orgs.find(orgId);
    if (it != m_orgs.end()) {
        if (std::find(it->second.members.begin(), it->second.members.end(), memberId) == it->second.members.end()) {
            it->second.members.push_back(memberId);

            PreparedStatement mStmt("INSERT INTO `crew_members` (`crew_id`, `member_goid`, `rank`) VALUES (?0, ?1, 2) ON DUPLICATE KEY UPDATE `rank`=2");
            mStmt.SetUInt32(0, orgId);
            mStmt.SetUInt32(1, (uint32)memberId);
            sDatabase.ExecutePrepared(&mStmt);

            return true;
        }
    }
    return false;
}

bool OrganizationManager::leaveOrganization(uint32 orgId, uint64 memberId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_orgs.find(orgId);
    if (it != m_orgs.end()) {
        auto& members = it->second.members;
        auto memberIt = std::find(members.begin(), members.end(), memberId);
        if (memberIt != members.end()) {
            members.erase(memberIt);

            PreparedStatement delStmt("DELETE FROM `crew_members` WHERE `crew_id` = ?0 AND `member_goid` = ?1");
            delStmt.SetUInt32(0, orgId);
            delStmt.SetUInt32(1, (uint32)memberId);
            sDatabase.ExecutePrepared(&delStmt);

            if (members.empty()) {
                PreparedStatement delCrew("DELETE FROM `crews` WHERE `id` = ?0");
                delCrew.SetUInt32(0, orgId);
                sDatabase.ExecutePrepared(&delCrew);
                m_orgs.erase(it);
            }
            return true;
        }
    }
    return false;
}

Organization* OrganizationManager::getOrganization(uint32 orgId) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_orgs.find(orgId);
    if (it != m_orgs.end()) {
        return &it->second;
    }
    return nullptr;
}

Organization* OrganizationManager::getOrganizationForPlayer(uint64 memberId) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    for (auto& pair : m_orgs) {
        if (std::find(pair.second.members.begin(), pair.second.members.end(), memberId) != pair.second.members.end()) {
            return &pair.second;
        }
    }
    return nullptr;
}
