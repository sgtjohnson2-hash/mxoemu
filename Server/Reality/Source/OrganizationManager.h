#ifndef MXOEMU_ORGANIZATIONMANAGER_H
#define MXOEMU_ORGANIZATIONMANAGER_H

#include "Common.h"
#include <map>
#include <string>
#include <vector>
#include <shared_mutex>

struct Organization {
    uint32 id;
    std::string name;
    uint64 leaderId; // charUID
    std::vector<uint64> members; // charUIDs
};

class OrganizationManager {
public:
    static OrganizationManager& getSingleton() {
        static OrganizationManager instance;
        return instance;
    }

    OrganizationManager() : m_nextOrgId(1) {}

    uint32 createOrganization(const std::string& name, uint64 leaderId);
    bool joinOrganization(uint32 orgId, uint64 memberId);
    bool leaveOrganization(uint32 orgId, uint64 memberId);
    Organization* getOrganization(uint32 orgId);

private:
    std::map<uint32, Organization> m_orgs;
    uint32 m_nextOrgId;
    mutable std::shared_mutex m_mutex;
};

#define sOrgMgr OrganizationManager::getSingleton()

#endif
