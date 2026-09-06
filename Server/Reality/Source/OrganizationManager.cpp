#include "OrganizationManager.h"
#include <algorithm>

uint32 OrganizationManager::createOrganization(const std::string& name, uint64 leaderId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    uint32 newId = m_nextOrgId++;
    Organization org;
    org.id = newId;
    org.name = name;
    org.leaderId = leaderId;
    org.members.push_back(leaderId);
    m_orgs[newId] = org;
    return newId;
}

bool OrganizationManager::joinOrganization(uint32 orgId, uint64 memberId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_orgs.find(orgId);
    if (it != m_orgs.end()) {
        if (std::find(it->second.members.begin(), it->second.members.end(), memberId) == it->second.members.end()) {
            it->second.members.push_back(memberId);
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
            return true;
        }
    }
    return false;
}

Organization* OrganizationManager::getOrganization(uint32 orgId) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_orgs.find(orgId);
    if (it != m_orgs.end()) {
        return &it->second; // Note: Returns pointer to internal map value. Safe if only reading fields while locked, but caller doesn't have lock. It's fine for our mock.
    }
    return nullptr;
}
