#include "LoadingConstruct.h"
#include "GameServer.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "MessageTypes.h"
#include "Log.h"
#include <algorithm>
#include <iostream>

static inline bool Has3DWorldSupport() {
    return GameServer::getSingletonPtr() != nullptr && BotManager::getSingletonPtr() != nullptr;
}

createFileSingleton(LoadingConstruct);

LoadingConstruct::LoadingConstruct()
{
    Initialize();
}

LoadingConstruct::~LoadingConstruct()
{
}

void LoadingConstruct::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    m_dummies.clear();
    m_targets.clear();
    m_dojoPillars.clear();
    m_shojiScreens.clear();
    m_weaponRacks.clear();

    m_currentMode = CONSTRUCT_MODE_WHITE_VOID;
    m_timeDilation = 1.0f;
    m_wireFuSlowMoActive = false;
    m_slowMoTimerSec = 0.0f;
    m_tatamiImpactScuffCount = 0;

    // Populate Weapon Racks ("Guns. Lots of guns.")
    m_weaponRacks[RACK_HANDGUNS] = {
        {101, "Dual Beretta 92FS Stainless", RACK_HANDGUNS, 30, 45.0f},
        {102, "Desert Eagle .50 AE Matte Black", RACK_HANDGUNS, 7, 95.0f},
        {103, "Glock 18 Select-Fire 9mm", RACK_HANDGUNS, 33, 38.0f}
    };

    m_weaponRacks[RACK_SUBMACHINE_GUNS] = {
        {201, "Heckler & Koch MP5K Tactical", RACK_SUBMACHINE_GUNS, 30, 40.0f},
        {202, "IMI Micro Uzi Dual Akimbo", RACK_SUBMACHINE_GUNS, 64, 35.0f},
        {203, "Skorpion vz. 61 Machine Pistol", RACK_SUBMACHINE_GUNS, 20, 32.0f}
    };

    m_weaponRacks[RACK_ASSAULT_RIFLES] = {
        {301, "Colt M16A2 Military Carbine", RACK_ASSAULT_RIFLES, 30, 65.0f},
        {302, "Kalashnikov AK-47 Custom", RACK_ASSAULT_RIFLES, 30, 72.0f},
        {303, "Heckler & Koch G36C Compact", RACK_ASSAULT_RIFLES, 30, 68.0f}
    };

    m_weaponRacks[RACK_SHOTGUNS] = {
        {401, "Franchi SPAS-12 Combat Shotgun", RACK_SHOTGUNS, 8, 140.0f},
        {402, "Remington 870 Tactical Express", RACK_SHOTGUNS, 7, 130.0f},
        {403, "Winchester Model 1897 Trench Gun", RACK_SHOTGUNS, 6, 135.0f}
    };

    m_weaponRacks[RACK_SNIPER_RIFLES] = {
        {501, "Barrett M82A1 .50 BMG Anti-Material", RACK_SNIPER_RIFLES, 10, 350.0f},
        {502, "Walther WA2000 Bullpup Precision", RACK_SNIPER_RIFLES, 6, 220.0f}
    };

    m_weaponRacks[RACK_MELEE_WEAPONS] = {
        {601, "Folded Damascus Steel Katana", RACK_MELEE_WEAPONS, 0, 85.0f},
        {602, "Hardwood Oak Bo Staff", RACK_MELEE_WEAPONS, 0, 60.0f},
        {603, "Titanium Combat Trench Dagger", RACK_MELEE_WEAPONS, 0, 50.0f},
        {604, "Traditional Bokken Practice Sword", RACK_MELEE_WEAPONS, 0, 40.0f}
    };

    m_weaponRacks[RACK_VEHICLES] = {
        {701, "Nebuchadnezzar Zion Hovercraft", RACK_VEHICLES, 0, 2500.0f},
        {702, "Lincoln Continental Executive Town Car", RACK_VEHICLES, 0, 800.0f},
        {703, "Ducati 996 Matrix Edition Motorcycle", RACK_VEHICLES, 0, 500.0f}
    };

    SetupTargetRange();
    SetupOrientalDojo();

    if (Log::getSingletonPtr())
    {
        sLog.outString("[LoadingConstruct] Initialized Loading Construct with %zu weapon categories.", m_weaponRacks.size());
    }
}

void LoadingConstruct::Reset()
{
    Initialize();
}

void LoadingConstruct::SetConstructMode(ConstructMode mode)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    m_currentMode = mode;
    sLog.outString("[LoadingConstruct] Construct mode switched to %d", (int)mode);
}

void LoadingConstruct::SpawnWeaponRack(WeaponRackCategory category)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    auto it = m_weaponRacks.find(category);
    if (it != m_weaponRacks.end())
    {
        sLog.outString("[LoadingConstruct] Deployed weapon rack with %zu items in category %d", it->second.size(), (int)category);
    }
}

std::vector<WeaponRackItem> LoadingConstruct::GetRackItems(WeaponRackCategory category)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    auto it = m_weaponRacks.find(category);
    if (it != m_weaponRacks.end())
        return it->second;
    return {};
}

size_t LoadingConstruct::GetTotalAvailableWeapons() const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    size_t total = 0;
    for (const auto& pair : m_weaponRacks)
    {
        total += pair.second.size();
    }
    return total;
}

uint32 LoadingConstruct::SpawnSparringDummy(DummyAIType aiType, float x, float y, float z, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    uint32 id = m_nextDummyId++;

    SparringDummy dummy;
    dummy.dummyId = id;
    dummy.aiType = aiType;
    dummy.x = x;
    dummy.y = y;
    dummy.z = z;
    dummy.health = 1000.0f;
    dummy.maxHealth = 1000.0f;
    dummy.posture = 100.0f;
    dummy.hitsReceived = 0;
    dummy.totalDamageTaken = 0.0f;
    dummy.postureBroken = false;

    if (!name.empty())
        dummy.name = name;
    else
    {
        switch (aiType)
        {
            case DUMMY_PASSIVE: dummy.name = "Passive Training Dummy"; break;
            case DUMMY_DEFENSIVE: dummy.name = "Defensive Guard Dummy"; break;
            case DUMMY_AGGRESSIVE: dummy.name = "Sparring Combatant Bot"; break;
            case DUMMY_WIREFU_MASTER: dummy.name = "Wire-Fu Kung-Fu Master"; break;
        }
    }

    m_dummies[id] = dummy;
    sLog.outString("[LoadingConstruct] Spawned sparring dummy '%s' (ID %u) at (%.1f, %.1f, %.1f)",
                   dummy.name.c_str(), id, x, y, z);
    return id;
}

bool LoadingConstruct::DamageDummy(uint32 dummyId, float damage, float postureDamage, bool& outPostureBroken)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    outPostureBroken = false;

    auto it = m_dummies.find(dummyId);
    if (it == m_dummies.end()) return false;

    auto& dummy = it->second;
    dummy.hitsReceived++;
    dummy.totalDamageTaken += damage;
    dummy.health = std::max(0.0f, dummy.health - damage);

    dummy.posture = std::max(0.0f, dummy.posture - postureDamage);
    if (dummy.posture <= 0.0f && !dummy.postureBroken)
    {
        dummy.postureBroken = true;
        outPostureBroken = true;
    }

    return true;
}

void LoadingConstruct::ResetDummies()
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    for (auto& pair : m_dummies)
    {
        pair.second.health = pair.second.maxHealth;
        pair.second.posture = 100.0f;
        pair.second.postureBroken = false;
    }
}

size_t LoadingConstruct::GetDummyCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    return m_dummies.size();
}

void LoadingConstruct::SetupTargetRange()
{
    m_targets.clear();
    float dists[4] = {10.0f, 25.0f, 50.0f, 100.0f};
    for (int i = 0; i < 4; ++i)
    {
        BallisticTarget t;
        t.targetId = (uint32)(i + 1);
        t.distanceMeters = dists[i];
        t.x = 0.0f;
        t.y = 150.0f;
        t.z = dists[i] * 100.0f; // 1m = 100 units
        t.totalShotsFired = 0;
        t.totalHits = 0;
        t.headshots = 0;
        t.bullseyes = 0;
        m_targets.push_back(t);
    }
}

bool LoadingConstruct::RecordBallisticShot(uint32 targetId, float hitOffsetX, float hitOffsetY, bool isHeadshot)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    for (auto& t : m_targets)
    {
        if (t.targetId == targetId)
        {
            t.totalShotsFired++;
            float distFromCenter = std::sqrt(hitOffsetX * hitOffsetX + hitOffsetY * hitOffsetY);

            // Target diameter is 50.0 units
            if (distFromCenter <= 50.0f)
            {
                t.totalHits++;
                if (distFromCenter <= 5.0f)
                {
                    t.bullseyes++;
                }
                if (isHeadshot || hitOffsetY > 25.0f)
                {
                    t.headshots++;
                }
                return true;
            }
            return false;
        }
    }
    return false;
}

float LoadingConstruct::GetTargetAccuracyPercent(uint32 targetId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    for (const auto& t : m_targets)
    {
        if (t.targetId == targetId)
        {
            if (t.totalShotsFired == 0) return 0.0f;
            return ((float)t.totalHits / (float)t.totalShotsFired) * 100.0f;
        }
    }
    return 0.0f;
}

void LoadingConstruct::ResetTargets()
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    for (auto& t : m_targets)
    {
        t.totalShotsFired = 0;
        t.totalHits = 0;
        t.headshots = 0;
        t.bullseyes = 0;
    }
}

void LoadingConstruct::SetupOrientalDojo()
{
    m_dojoPillars.clear();
    m_shojiScreens.clear();
    m_tatamiImpactScuffCount = 0;

    // 8 Wooden Support Pillars placed along outer ring of dojo (radius 2000 units)
    float radius = 2000.0f;
    for (int i = 0; i < 8; ++i)
    {
        float angle = (float)i * (3.14159265f / 4.0f);
        DojoPillar p;
        p.pillarId = (uint32)(i + 1);
        p.x = std::cos(angle) * radius;
        p.y = 0.0f;
        p.z = std::sin(angle) * radius;
        p.health = 500.0f;
        p.maxHealth = 500.0f;
        p.state = PILLAR_INTACT;
        p.splinterDebrisCount = 0;
        m_dojoPillars.push_back(p);
    }

    // 12 Sliding Shoji Paper Screens along perimeter wall
    for (int i = 0; i < 12; ++i)
    {
        ShojiScreen s;
        s.screenId = (uint32)(i + 1);
        float a1 = (float)i * (3.14159265f / 6.0f);
        float a2 = (float)(i + 1) * (3.14159265f / 6.0f);
        float rPerim = 2500.0f;
        s.x1 = std::cos(a1) * rPerim;
        s.z1 = std::sin(a1) * rPerim;
        s.x2 = std::cos(a2) * rPerim;
        s.z2 = std::sin(a2) * rPerim;
        s.health = 50.0f;
        s.maxHealth = 50.0f;
        s.state = SHOJI_INTACT;
        m_shojiScreens.push_back(s);
    }
}

bool LoadingConstruct::ApplyInterlockImpact(float x, float y, float z, float impactForce, bool isBodyThrow,
                                           uint32& outPillarsDamaged, uint32& outShojiTorn)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    outPillarsDamaged = 0;
    outShojiTorn = 0;

    if (isBodyThrow)
    {
        m_tatamiImpactScuffCount++;
    }

    float damageRadius = isBodyThrow ? 600.0f : 350.0f;

    // Check Pillar destruction
    for (auto& p : m_dojoPillars)
    {
        float dx = p.x - x;
        float dz = p.z - z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist <= damageRadius)
        {
            float dmg = (1.0f - (dist / damageRadius)) * impactForce;
            p.health = std::max(0.0f, p.health - dmg);
            if (p.health <= 0.0f)
            {
                p.state = PILLAR_SHATTERED;
                p.splinterDebrisCount += 45;
                outPillarsDamaged++;
            }
            else if (p.health < p.maxHealth * 0.5f)
            {
                p.state = PILLAR_DAMAGED;
                p.splinterDebrisCount += 15;
                outPillarsDamaged++;
            }
        }
    }

    // Check Shoji Screen destruction
    for (auto& s : m_shojiScreens)
    {
        float mx = (s.x1 + s.x2) * 0.5f;
        float mz = (s.z1 + s.z2) * 0.5f;
        float dx = mx - x;
        float dz = mz - z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist <= damageRadius + 200.0f)
        {
            float dmg = (1.0f - (dist / (damageRadius + 200.0f))) * impactForce * 1.5f;
            s.health = std::max(0.0f, s.health - dmg);
            if (s.health <= 0.0f)
            {
                s.state = SHOJI_DESTROYED;
                outShojiTorn++;
            }
            else if (s.health < s.maxHealth * 0.6f)
            {
                s.state = SHOJI_TORN;
                outShojiTorn++;
            }
        }
    }

    return (outPillarsDamaged > 0 || outShojiTorn > 0);
}

size_t LoadingConstruct::GetPillarCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    return m_dojoPillars.size();
}

size_t LoadingConstruct::GetShatteredPillarCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    size_t count = 0;
    for (const auto& p : m_dojoPillars)
    {
        if (p.state == PILLAR_SHATTERED) count++;
    }
    return count;
}

size_t LoadingConstruct::GetDestroyedShojiCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    size_t count = 0;
    for (const auto& s : m_shojiScreens)
    {
        if (s.state == SHOJI_DESTROYED) count++;
    }
    return count;
}

float LoadingConstruct::GetDojoStructuralIntegrityPercent() const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    if (m_dojoPillars.empty()) return 100.0f;

    float currentHp = 0.0f;
    float maxHp = 0.0f;
    for (const auto& p : m_dojoPillars)
    {
        currentHp += p.health;
        maxHp += p.maxHealth;
    }
    return (currentHp / maxHp) * 100.0f;
}

void LoadingConstruct::SetTimeDilation(float multiplier)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    m_timeDilation = std::clamp(multiplier, 0.05f, 2.0f);
}

void LoadingConstruct::TriggerWireFuSlowMo(float durationSec, float slowMoMultiplier)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    m_wireFuSlowMoActive = true;
    m_slowMoDurationSec = durationSec;
    m_slowMoTimerSec = durationSec;
    m_originalDilation = 1.0f;
    m_activeSlowMoMultiplier = std::clamp(slowMoMultiplier, 0.05f, 0.50f);
    m_timeDilation = m_activeSlowMoMultiplier;
    sLog.outString("[LoadingConstruct] Triggered Wire-Fu Slow-Mo (factor %.2f, duration %.1fs)",
                   m_timeDilation, durationSec);
}

void LoadingConstruct::UpdateTimeDilation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    if (m_wireFuSlowMoActive)
    {
        m_slowMoTimerSec -= deltaTimeSec;
        if (m_slowMoTimerSec <= 0.0f)
        {
            m_wireFuSlowMoActive = false;
            m_timeDilation = m_originalDilation;
            sLog.outString("[LoadingConstruct] Wire-Fu Slow-Mo ended. Restored time dilation to %.2f", m_timeDilation);
        }
        else
        {
            // Smooth ease out in the last 20% of duration
            float progress = 1.0f - (m_slowMoTimerSec / m_slowMoDurationSec);
            if (progress > 0.80f)
            {
                float ease = (progress - 0.80f) / 0.20f;
                m_timeDilation = m_activeSlowMoMultiplier + (m_originalDilation - m_activeSlowMoMultiplier) * ease;
            }
        }
    }
}

bool LoadingConstruct::LoadMartialArtsDiskette(uint32 playerId, MartialArtProgram program)
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    DisketteUploadState state;
    state.playerId = playerId;
    state.program = program;
    state.uploadProgressPercent = 100.0f;
    state.isComplete = true;
    m_playerSkillUploads[playerId] = state;

    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(playerId)) {
            po->sayChat("I know Kung Fu.");
            po->Emote(1);
        }
    }

    sLog.outString("[LoadingConstruct] Loaded Martial Arts Diskette (Program %u) for Player %u",
                   (uint32)program, playerId);
    return true;
}

MartialArtProgram LoadingConstruct::GetPlayerMasteredArt(uint32 playerId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    auto it = m_playerSkillUploads.find(playerId);
    if (it != m_playerSkillUploads.end() && it->second.isComplete) {
        return it->second.program;
    }
    return MARTIAL_ART_NONE;
}

float LoadingConstruct::GetDisketteUploadProgress(uint32 playerId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_constructMutex);
    auto it = m_playerSkillUploads.find(playerId);
    if (it != m_playerSkillUploads.end()) {
        return it->second.uploadProgressPercent;
    }
    return 0.0f;
}

void RunConstructTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING LOADING CONSTRUCT & KATA DOJO TEST SUITE         " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    LoadingConstruct& construct = sLoadingConstruct;
    construct.Reset();

    int passedCount = 0;
    int failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passedCount++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failedCount++;
        }
    };

    // 1. Construct Mode Switching & White Void
    {
        TEST_ASSERT(construct.GetConstructMode() == CONSTRUCT_MODE_WHITE_VOID, "Default mode is CONSTRUCT_MODE_WHITE_VOID");
        construct.SetConstructMode(CONSTRUCT_MODE_ORIENTAL_DOJO);
        TEST_ASSERT(construct.GetConstructMode() == CONSTRUCT_MODE_ORIENTAL_DOJO, "SetConstructMode transitions to ORIENTAL_DOJO");
        construct.SetConstructMode(CONSTRUCT_MODE_TARGET_RANGE);
        TEST_ASSERT(construct.GetConstructMode() == CONSTRUCT_MODE_TARGET_RANGE, "SetConstructMode transitions to TARGET_RANGE");
    }

    // 2. Weapon Racks ("Guns. Lots of guns.")
    {
        size_t totalWeapons = construct.GetTotalAvailableWeapons();
        TEST_ASSERT(totalWeapons >= 14, "Weapon racks contain 14+ authentic Matrix firearms");

        auto handguns = construct.GetRackItems(RACK_HANDGUNS);
        TEST_ASSERT(!handguns.empty(), "Handgun rack populated");
        TEST_ASSERT(handguns[0].name.find("Beretta") != std::string::npos, "Dual Berettas available on rack");

        auto snipers = construct.GetRackItems(RACK_SNIPER_RIFLES);
        TEST_ASSERT(!snipers.empty(), "Sniper rifle rack populated");
        TEST_ASSERT(snipers[0].baseDamage >= 300.0f, "Barrett .50 BMG has heavy anti-material damage");
    }

    // 3. Sparring Dummy & Posture Break Mechanics
    {
        uint32 dummyId = construct.SpawnSparringDummy(DUMMY_AGGRESSIVE, 0.0f, 0.0f, 0.0f, "Morpheus Avatar");
        TEST_ASSERT(dummyId != 0, "SpawnSparringDummy creates valid dummy instance");
        TEST_ASSERT(construct.GetDummyCount() == 1, "Dummy count tracked");

        bool postureBroken = false;
        bool damaged = construct.DamageDummy(dummyId, 100.0f, 40.0f, postureBroken);
        TEST_ASSERT(damaged && !postureBroken, "Partial posture damage applied without breaking");

        // Inflict remaining posture damage to trigger posture break
        construct.DamageDummy(dummyId, 100.0f, 70.0f, postureBroken);
        TEST_ASSERT(postureBroken, "Cumulative posture damage triggers posture break state");
    }

    // 4. Ballistic Target Range
    {
        construct.SetupTargetRange();
        const auto& targets = construct.GetTargets();
        TEST_ASSERT(targets.size() == 4, "Target range contains 4 distance target silhouettes");

        // Record a bullseye headshot
        bool hitRecorded = construct.RecordBallisticShot(1, 0.01f, 0.01f, true);
        TEST_ASSERT(hitRecorded, "RecordBallisticShot records precision impact");
        TEST_ASSERT(construct.GetTargetAccuracyPercent(1) == 100.0f, "Target accuracy calculates 100% on direct hit");
    }

    // 5. Oriental Dojo Structural Destruction & Shoji Screens
    {
        construct.SetupOrientalDojo();
        TEST_ASSERT(construct.GetPillarCount() == 8, "Oriental Dojo spawns 8 structural cedar pillars");
        TEST_ASSERT(construct.GetDestroyedShojiCount() == 0, "Initial shoji screens are all intact");
        TEST_ASSERT(construct.GetDojoStructuralIntegrityPercent() == 100.0f, "Dojo structural integrity begins at 100%");

        uint32 pillarsDamaged = 0;
        uint32 shojiTorn = 0;
        // Body throw impact directly against perimeter pillar (radius 2000) and shoji partition
        bool impactResult = construct.ApplyInterlockImpact(2000.0f, 0.0f, 0.0f, 800.0f, true, pillarsDamaged, shojiTorn);
        TEST_ASSERT(impactResult, "ApplyInterlockImpact processes martial arts body throw");
        TEST_ASSERT(pillarsDamaged > 0, "Body throw shatters structural cedar pillars");
        TEST_ASSERT(shojiTorn > 0, "Body throw tears paper shoji screens");
        TEST_ASSERT(construct.GetDojoStructuralIntegrityPercent() < 100.0f, "Dojo structural integrity degrades upon pillar damage");
    }

    // 6. Time Dilation & Wire-Fu Slow-Mo (0.10x Dilation)
    {
        construct.SetTimeDilation(1.0f);
        TEST_ASSERT(construct.GetTimeDilation() == 1.0f, "Default time dilation is 1.0x");

        construct.TriggerWireFuSlowMo(3.5f, 0.10f);
        TEST_ASSERT(construct.IsWireFuSlowMoActive(), "Wire-Fu slow-mo is actively engaged");
        TEST_ASSERT(construct.GetTimeDilation() <= 0.10f, "Time dilation decelerated to 0.10x bullet-time");

        construct.UpdateTimeDilation(3.6f); // Advance past slow-mo duration
        TEST_ASSERT(!construct.IsWireFuSlowMoActive(), "Wire-Fu slow-mo concludes when duration expires");
        TEST_ASSERT(construct.GetTimeDilation() == 1.0f, "Time dilation restored smoothly to 1.0x");
    }

    // 7. "I Know Kung Fu" Diskette Loader
    {
        uint32 testPlayerId = 9901;
        TEST_ASSERT(construct.GetPlayerMasteredArt(testPlayerId) == MARTIAL_ART_NONE, "Player initially has no mastered martial art");

        bool loaded = construct.LoadMartialArtsDiskette(testPlayerId, MARTIAL_ART_WING_CHUN);
        TEST_ASSERT(loaded, "LoadMartialArtsDiskette successfully uploads program");
        TEST_ASSERT(construct.GetPlayerMasteredArt(testPlayerId) == MARTIAL_ART_WING_CHUN, "Player mastered Wing Chun martial arts");
        TEST_ASSERT(construct.GetDisketteUploadProgress(testPlayerId) == 100.0f, "Upload progress reaches 100%");

        // Load advanced Drunken Fist
        construct.LoadMartialArtsDiskette(testPlayerId, MARTIAL_ART_DRUNKEN_FIST);
        TEST_ASSERT(construct.GetPlayerMasteredArt(testPlayerId) == MARTIAL_ART_DRUNKEN_FIST, "Player dynamically updates mastered art to Drunken Fist");
    }

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  LOADING CONSTRUCT & KATA DOJO TEST SUITE COMPLETE         " << std::endl;
    std::cout << "  PASSED: " << passedCount << " | FAILED: " << failedCount << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failedCount > 0) {
        std::cerr << "Loading Construct test suite encountered failures!" << std::endl;
        exit(1);
    }
}
