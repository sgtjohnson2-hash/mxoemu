#ifndef MXOEMU_LOADING_CONSTRUCT_H
#define MXOEMU_LOADING_CONSTRUCT_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum ConstructMode
{
    CONSTRUCT_MODE_WHITE_VOID = 0,
    CONSTRUCT_MODE_ORIENTAL_DOJO = 1,
    CONSTRUCT_MODE_TARGET_RANGE = 2,
    CONSTRUCT_MODE_OBSTACLE_COURSE = 3
};

enum DummyAIType
{
    DUMMY_PASSIVE = 0,
    DUMMY_DEFENSIVE = 1,
    DUMMY_AGGRESSIVE = 2,
    DUMMY_WIREFU_MASTER = 3
};

enum PillarState
{
    PILLAR_INTACT = 0,
    PILLAR_DAMAGED = 1,
    PILLAR_SHATTERED = 2
};

enum ShojiScreenState
{
    SHOJI_INTACT = 0,
    SHOJI_TORN = 1,
    SHOJI_DESTROYED = 2
};

enum WeaponRackCategory
{
    RACK_HANDGUNS = 0,
    RACK_SUBMACHINE_GUNS = 1,
    RACK_ASSAULT_RIFLES = 2,
    RACK_SHOTGUNS = 3,
    RACK_SNIPER_RIFLES = 4,
    RACK_MELEE_WEAPONS = 5,
    RACK_VEHICLES = 6
};

enum MartialArtProgram
{
    MARTIAL_ART_NONE = 0,
    MARTIAL_ART_WING_CHUN = 1,
    MARTIAL_ART_JIU_JITSU = 2,
    MARTIAL_ART_SAVATE = 3,
    MARTIAL_ART_DRUNKEN_FIST = 4,
    MARTIAL_ART_KENJUTSU = 5
};

struct DisketteUploadState
{
    uint32 playerId{0};
    MartialArtProgram program{MARTIAL_ART_NONE};
    float uploadProgressPercent{0.0f};
    bool isComplete{false};
};

struct SparringDummy
{
    uint32 dummyId{0};
    std::string name{"Sparring Dummy"};
    DummyAIType aiType{DUMMY_PASSIVE};
    float x{0.0f}, y{0.0f}, z{0.0f};
    float health{1000.0f};
    float maxHealth{1000.0f};
    float posture{100.0f}; // 0 to 100
    uint32 hitsReceived{0};
    float totalDamageTaken{0.0f};
    bool postureBroken{false};
};

struct WeaponRackItem
{
    uint32 itemId{0};
    std::string name;
    WeaponRackCategory category;
    uint32 ammoCapacity{0};
    float baseDamage{0.0f};
};

struct DojoPillar
{
    uint32 pillarId{0};
    float x{0.0f}, y{0.0f}, z{0.0f};
    float health{500.0f};
    float maxHealth{500.0f};
    PillarState state{PILLAR_INTACT};
    uint32 splinterDebrisCount{0};
};

struct ShojiScreen
{
    uint32 screenId{0};
    float x1{0.0f}, z1{0.0f};
    float x2{0.0f}, z2{0.0f};
    float health{50.0f};
    float maxHealth{50.0f};
    ShojiScreenState state{SHOJI_INTACT};
};

struct BallisticTarget
{
    uint32 targetId{0};
    float distanceMeters{10.0f};
    float x{0.0f}, y{0.0f}, z{0.0f};
    uint32 totalShotsFired{0};
    uint32 totalHits{0};
    uint32 headshots{0};
    uint32 bullseyes{0};
};

class LoadingConstruct : public Singleton<LoadingConstruct>
{
public:
    LoadingConstruct();
    ~LoadingConstruct();

    void Initialize();
    void Reset();

    // Mode Selection
    void SetConstructMode(ConstructMode mode);
    ConstructMode GetConstructMode() const { return m_currentMode; }

    // Weapon & Vehicle Racks ("Guns. Lots of guns.")
    void SpawnWeaponRack(WeaponRackCategory category);
    std::vector<WeaponRackItem> GetRackItems(WeaponRackCategory category);
    size_t GetTotalAvailableWeapons() const;

    // Sparring Dummy Bots
    uint32 SpawnSparringDummy(DummyAIType aiType, float x, float y, float z, const std::string& name = "");
    bool DamageDummy(uint32 dummyId, float damage, float postureDamage, bool& outPostureBroken);
    void ResetDummies();
    size_t GetDummyCount() const;
    const std::map<uint32, SparringDummy>& GetDummies() const { return m_dummies; }

    // Ballistic Target Range
    void SetupTargetRange();
    bool RecordBallisticShot(uint32 targetId, float hitOffsetX, float hitOffsetY, bool isHeadshot);
    float GetTargetAccuracyPercent(uint32 targetId) const;
    void ResetTargets();
    const std::vector<BallisticTarget>& GetTargets() const { return m_targets; }

    // Oriental Sparring Dojo Instance & Procedural Destruction
    void SetupOrientalDojo();
    bool ApplyInterlockImpact(float x, float y, float z, float impactForce, bool isBodyThrow,
                              uint32& outPillarsDamaged, uint32& outShojiTorn);
    size_t GetPillarCount() const;
    size_t GetShatteredPillarCount() const;
    size_t GetDestroyedShojiCount() const;
    float GetDojoStructuralIntegrityPercent() const;

    // Time Dilation & Wire-Fu Choreography Mode
    void SetTimeDilation(float multiplier);
    float GetTimeDilation() const { return m_timeDilation; }
    void TriggerWireFuSlowMo(float durationSec, float slowMoMultiplier = 0.10f);
    void UpdateTimeDilation(float deltaTimeSec);
    bool IsWireFuSlowMoActive() const { return m_wireFuSlowMoActive; }

    // "I Know Kung Fu" Diskette Loader
    bool LoadMartialArtsDiskette(uint32 playerId, MartialArtProgram program);
    MartialArtProgram GetPlayerMasteredArt(uint32 playerId) const;
    float GetDisketteUploadProgress(uint32 playerId) const;

private:
    mutable std::recursive_mutex m_constructMutex;
    ConstructMode m_currentMode{CONSTRUCT_MODE_WHITE_VOID};

    // Weapon inventory racks
    std::map<WeaponRackCategory, std::vector<WeaponRackItem>> m_weaponRacks;

    // Dummies
    std::map<uint32, SparringDummy> m_dummies;
    uint32 m_nextDummyId{1};

    // Target Range
    std::vector<BallisticTarget> m_targets;

    // Dojo Props
    std::vector<DojoPillar> m_dojoPillars;
    std::vector<ShojiScreen> m_shojiScreens;
    uint32 m_tatamiImpactScuffCount{0};

    // Time Dilation
    float m_timeDilation{1.0f}; // Clamped between 0.05f and 2.0f
    bool m_wireFuSlowMoActive{false};
    float m_slowMoTimerSec{0.0f};
    float m_slowMoDurationSec{0.0f};
    float m_originalDilation{1.0f};
    float m_activeSlowMoMultiplier{0.10f};

    // Skill Uploads
    std::map<uint32, DisketteUploadState> m_playerSkillUploads;
};

#define sLoadingConstruct LoadingConstruct::getSingleton()

void RunConstructTestSuite();

#endif // MXOEMU_LOADING_CONSTRUCT_H
