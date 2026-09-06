#ifndef MXOEMU_HOVERCRAFTSYSTEM_H
#define MXOEMU_HOVERCRAFTSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include "PlayerObject.h"
#include <map>
#include <mutex>

struct HovercraftUpgrade
{
    uint32 level;
    uint32 totalInfoDonated;
    uint32 jackOutTimeMS;
};

class HovercraftSystem : public Singleton<HovercraftSystem>
{
public:
    HovercraftSystem();
    ~HovercraftSystem();

    void Initialize();
    bool DonateInfo(PlayerObject* player, uint32 amount);
    uint32 GetJackOutTime() const;

private:
    void CheckForUpgrades();

    std::mutex m_mutex;
    HovercraftUpgrade m_currentStats;
};

#define sHovercraftSys HovercraftSystem::getSingleton()

#endif // MXOEMU_HOVERCRAFTSYSTEM_H
