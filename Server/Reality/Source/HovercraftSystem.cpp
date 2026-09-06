#include "HovercraftSystem.h"
#include "Log.h"

createFileSingleton(HovercraftSystem);

HovercraftSystem::HovercraftSystem()
{
}

HovercraftSystem::~HovercraftSystem()
{
}

void HovercraftSystem::Initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentStats.level = 1;
    m_currentStats.totalInfoDonated = 0;
    m_currentStats.jackOutTimeMS = 15000; // Base 15 seconds
    INFO_LOG("HovercraftSystem Initialized with level 1.");
}

bool HovercraftSystem::DonateInfo(PlayerObject* player, uint32 amount)
{
    if (!player) return false;

    if (player->getInfo() < amount) {
        return false;
    }

    player->removeInfo(amount);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentStats.totalInfoDonated += amount;

    INFO_LOG(format("Player %1% donated %2% Info to Hovercraft. Total: %3%") % player->getHandle() % amount % m_currentStats.totalInfoDonated);
    
    CheckForUpgrades();
    return true;
}

void HovercraftSystem::CheckForUpgrades()
{
    // Thresholds:
    // Level 2: 500 Info -> 10 seconds JackOut
    // Level 3: 2000 Info -> 5 seconds JackOut
    // Level 4: 5000 Info -> 2 seconds JackOut
    if (m_currentStats.level == 1 && m_currentStats.totalInfoDonated >= 500) {
        m_currentStats.level = 2;
        m_currentStats.jackOutTimeMS = 10000;
        INFO_LOG("Hovercraft upgraded to Level 2! Jack-Out time reduced to 10s.");
    }
    else if (m_currentStats.level == 2 && m_currentStats.totalInfoDonated >= 2000) {
        m_currentStats.level = 3;
        m_currentStats.jackOutTimeMS = 5000;
        INFO_LOG("Hovercraft upgraded to Level 3! Jack-Out time reduced to 5s.");
    }
    else if (m_currentStats.level == 3 && m_currentStats.totalInfoDonated >= 5000) {
        m_currentStats.level = 4;
        m_currentStats.jackOutTimeMS = 2000;
        INFO_LOG("Hovercraft upgraded to Level 4! Jack-Out time reduced to 2s.");
    }
}

uint32 HovercraftSystem::GetJackOutTime() const
{
    // We don't strictly need a lock for a 32-bit read, but being safe
    return m_currentStats.jackOutTimeMS;
}
