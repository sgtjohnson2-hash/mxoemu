#include "DataLoader.h"
#include "Log.h"
#include "MissionSystem.h"
#include <fstream>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

createFileSingleton(DataLoader);

static unsigned long safe_stoul(const std::string& str) {
    if (str.empty()) return 0;
    try {
        return std::stoul(str);
    } catch (...) {
        return 0;
    }
}

static float safe_stof(const std::string& str) {
    if (str.empty()) return 0.0f;
    try {
        return std::stof(str);
    } catch (...) {
        return 0.0f;
    }
}

DataLoader::DataLoader()
{
}

DataLoader::~DataLoader()
{
}

static ObjectiveCommand ParseObjectiveCommand(const std::string& cmd)
{
    if (cmd == "TALK") return ObjectiveCommand::TALK;
    if (cmd == "DEFEAT") return ObjectiveCommand::DEFEAT;
    if (cmd == "LOOT") return ObjectiveCommand::LOOT;
    if (cmd == "GIVE") return ObjectiveCommand::GIVE;
    return ObjectiveCommand::TALK; // Default
}

bool DataLoader::LoadMissions(const std::string& directoryPath)
{
    sMissionSys.LoadMissionsFromXML(directoryPath);
    return true;
}

bool DataLoader::LoadAll(const std::string& directoryPath)
{
    // Try loading available data files.
    LoadClothing(directoryPath + "mxoClothing.csv");
    LoadAbilities(directoryPath + "abilityIDs.csv");
    EnsureCoreAbilities();
    LoadNPCs(directoryPath + "mob_parsed.csv");
    LoadBlueprints(directoryPath + "blueprints.csv");
    LoadPropheticGlitchNodes(directoryPath + "prophetic_glitch_nodes.csv");

    // Flyweight Personality Generation
    for (uint32 i = 0; i < 256; ++i) {
        m_personalities.push_back(BotPersonality::Generate(i + 1337));
    }

    INFO_LOG(format("DataLoader initialized. Loaded %1% items, %2% abilities, %3% NPCs, and %4% Glitch Nodes.") 
        % m_items.size() % m_abilities.size() % m_npcs.size() % m_glitchNodes.size());
    return true;
}

const BotPersonality* DataLoader::GetPersonalityProfile(uint32 index) const
{
    static const BotPersonality defaultPersonality = BotPersonality::Generate(0x1337);
    if (m_personalities.empty()) return &defaultPersonality;
    return &m_personalities[index % m_personalities.size()];
}

std::vector<std::string> DataLoader::SplitCSVLine(const std::string& line)
{
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string item;
    // mxoClothing uses ';' but we also want to support ',' for future dumps
    char delimiter = line.find(';') != std::string::npos ? ';' : ',';

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

bool DataLoader::LoadClothing(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
    {
        WARNING_LOG(format("Could not open clothing data file: %1%") % filePath);
        return false;
    }

    std::string line;
    bool isFirstLine = true;
    while (std::getline(file, line))
    {
        if (isFirstLine) 
        {
            isFirstLine = false; // Skip header
            continue;
        }

        auto tokens = SplitCSVLine(line);
        if (tokens.size() >= 7)
        {
            try {
                uint32 id = safe_stoul(tokens[0]);
            std::string name = tokens[5];
            
            ItemTemplate templ;
            templ.templateId = id;
            templ.type = ITEM_TYPE_CLOTHING;
            templ.name = name; 
            templ.rsiDataId = safe_stoul(tokens[6]);
            
            // Item 35: Clothing Stat Overhaul
            if (tokens.size() > 8) {
                templ.bonusHacking = safe_stoul(tokens[7]);
                templ.bonusEvasion = safe_stoul(tokens[8]);
            } else {
                // Fallback: assign pseudo-random stats based on ID to simulate the overhaul
                templ.bonusHacking = (id % 10 == 0) ? (id % 5) + 1 : 0;
                templ.bonusEvasion = (id % 10 == 1) ? (id % 5) + 1 : 0;
            }
            
            m_items[id] = templ;
            } catch (const std::exception& e) {
                WARNING_LOG(format("DataLoader: Failed to parse clothing line: %1%") % e.what());
            }
        }
    }
    return true;
}

bool DataLoader::LoadAbilities(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
        file.open("Data/hd_dump/abilityIDs.csv");
    if (!file.is_open())
        file.open("mxoemu_live/Server/Reality/Data/hd_dump/abilityIDs.csv");
    if (!file.is_open())
        file.open("../../Data/hd_dump/abilityIDs.csv");
    if (!file.is_open())
        file.open("../Data/hd_dump/abilityIDs.csv");
    if (!file.is_open())
    {
        WARNING_LOG(format("Could not open abilities data file: %1%") % filePath);
        EnsureCoreAbilities();
        return false;
    }

    std::string line;
    bool isFirstLine = true;
    while (std::getline(file, line))
    {
        if (isFirstLine) 
        {
            isFirstLine = false; // Skip header
            continue;
        }

        auto tokens = SplitCSVLine(line);
        if (tokens.size() >= 14) // Authentic schema has 14 columns
        {
            try {
                uint16 id = (uint16)safe_stoul(tokens[0]);
            
            AbilityTemplate templ;
            templ.abilityId = id;
            templ.goId = std::stol(tokens[1]);
            templ.name = tokens[2];
            templ.isCastable = (tokens[3] == "True" || tokens[3] == "true" || tokens[3] == "1");
            templ.castTime = safe_stoul(tokens[4]);
            templ.activationFX = safe_stoul(tokens[8]);
            templ.isBuff = (tokens[11] == "True" || tokens[11] == "true" || tokens[11] == "1");
            templ.buffTime = safe_stoul(tokens[12]);
            templ.executionFX = safe_stoul(tokens[13]);
            
            // Default fallback stats
            templ.description = "Authentic Ability";
            templ.memoryCost = 10;
            
            // Basic Attacks (1) should not cost IS or have long cooldowns
            if (templ.abilityId == 1) {
                templ.innerStrengthCost = 0;
                templ.cooldown = 0;
            } else {
                templ.innerStrengthCost = 15;
                templ.cooldown = 2000;
            }

            templ.maxLevel = 1;
            // Identify Abilities based on authentic retail discipline taxonomy
            if (templ.name.find("Virus") != std::string::npos || 
                templ.name.find("Logic") != std::string::npos ||
                templ.name.find("Nuke") != std::string::npos ||
                templ.name.find("Firewall") != std::string::npos ||
                templ.name.find("Freeze") != std::string::npos ||
                templ.name.find("Bomb") != std::string::npos ||
                templ.name.find("Phage") != std::string::npos ||
                templ.name.find("Overheat") != std::string::npos ||
                templ.name.find("Overload") != std::string::npos ||
                templ.name.find("Disrupt") != std::string::npos ||
                templ.name.find("Lag") != std::string::npos ||
                templ.name.find("Hacker") != std::string::npos ||
                templ.name.find("Drain") != std::string::npos ||
                templ.name.find("DeflectVirus") != std::string::npos)
            {
                templ.discipline = DisciplineType::HACKER;
            }
            else if (templ.name.find("Repair") != std::string::npos ||
                     templ.name.find("Heal") != std::string::npos ||
                     templ.name.find("Restore") != std::string::npos ||
                     templ.name.find("Revive") != std::string::npos ||
                     templ.name.find("Bolster") != std::string::npos ||
                     templ.name.find("Simulacra") != std::string::npos ||
                     templ.name.find("Coder") != std::string::npos ||
                     templ.name.find("Decoy") != std::string::npos ||
                     templ.name.find("Tinkering") != std::string::npos ||
                     templ.name.find("DeflectCode") != std::string::npos)
            {
                templ.discipline = DisciplineType::CODER;
            }
            else if (templ.name.find("KungFu") != std::string::npos ||
                     templ.name.find("Karate") != std::string::npos ||
                     templ.name.find("Aikido") != std::string::npos ||
                     templ.name.find("MartialArts") != std::string::npos ||
                     templ.name.find("CloseCombat") != std::string::npos ||
                     templ.name.find("SelfDefense") != std::string::npos ||
                     templ.name.find("Head Butt") != std::string::npos ||
                     templ.name.find("Cheap Shot") != std::string::npos)
            {
                templ.discipline = DisciplineType::MARTIAL_ARTIST;
            }
            else if (templ.name.find("Soldier") != std::string::npos ||
                     templ.name.find("Gunman") != std::string::npos ||
                     templ.name.find("Gunner") != std::string::npos ||
                     templ.name.find("Handgun") != std::string::npos ||
                     templ.name.find("Pistol") != std::string::npos ||
                     templ.name.find("Rifle") != std::string::npos ||
                     templ.name.find("Sniper") != std::string::npos ||
                     templ.name.find("Submachine") != std::string::npos ||
                     templ.name.find("PowerShot") != std::string::npos)
            {
                templ.discipline = DisciplineType::GUNNER;
            }
            else if (templ.name.find("Spy") != std::string::npos ||
                     templ.name.find("Stealth") != std::string::npos ||
                     templ.name.find("Knife") != std::string::npos ||
                     templ.name.find("Dagger") != std::string::npos ||
                     templ.name.find("Conceal") != std::string::npos ||
                     templ.name.find("Sabotage") != std::string::npos)
            {
                templ.discipline = DisciplineType::SPY;
            }
            else
            {
                templ.discipline = DisciplineType::NONE;
            }
            
            m_abilities[id] = templ;
            } catch (const std::exception& e) {
                WARNING_LOG(format("DataLoader: Failed to parse ability line: %1%") % e.what());
            }
        }
    }
    EnsureCoreAbilities();
    return true;
}

void DataLoader::EnsureCoreAbilities()
{
    auto addCore = [this](uint16 id, const std::string& name, DisciplineType disc, bool castable,
                          uint16 castTimeMs, uint16 isCost, uint32 actFx, uint32 execFx, bool isBuff, uint32 buffSec)
    {
        auto it = m_abilities.find(id);
        if (it == m_abilities.end())
        {
            AbilityTemplate t;
            t.abilityId = id;
            t.goId = -2147400000 + id;
            t.name = name;
            t.description = "Authentic Retail Ability";
            t.discipline = disc;
            t.isCastable = castable;
            t.castTime = castTimeMs;
            t.innerStrengthCost = isCost;
            t.memoryCost = 10;
            t.cooldown = 2000;
            t.maxLevel = 1;
            t.activationFX = actFx;
            t.executionFX = execFx;
            t.isBuff = isBuff;
            t.buffTime = buffSec;
            m_abilities[id] = t;
        }
        else
        {
            it->second.discipline = disc;
            it->second.isCastable = castable;
            if (isCost > 0) it->second.innerStrengthCost = isCost;
            if (castTimeMs > 0) it->second.castTime = castTimeMs;
            if (actFx != 0) it->second.activationFX = actFx;
            if (execFx != 0) it->second.executionFX = execFx;
        }
    };

    // Hacker Discipline
    addCore(57, "LogicBlast1Ability", DisciplineType::HACKER, true, 2000, 15, 671089111, 0xAF050028, false, 0);
    addCore(58, "LogicBlast2Ability", DisciplineType::HACKER, true, 2000, 20, 671089087, 0xAF050028, false, 0);
    addCore(59, "LogicBlast3Ability", DisciplineType::HACKER, true, 2000, 25, 671091200, 0xAF050028, false, 0);
    addCore(60, "LogicBomb1Ability", DisciplineType::HACKER, true, 4000, 30, 671090676, 671091267, false, 0);
    addCore(359, "CodeNukeAbility", DisciplineType::HACKER, true, 4000, 50, 671091217, 0x110A0028, false, 0);
    addCore(53, "HarmfulCodeAbility", DisciplineType::HACKER, true, 4000, 20, 671089139, 0, false, 0);
    addCore(40, "CodeFreeze1Ability", DisciplineType::HACKER, true, 2000, 20, 671088783, 0, false, 0);
    addCore(68, "PersonalFirewall1Ability", DisciplineType::HACKER, true, 2000, 25, 671089003, 0, true, 45);
    addCore(63, "NetworkFirewall1Ability", DisciplineType::HACKER, true, 6000, 45, 671089032, 671091263, true, 60);
    addCore(43, "DisruptInputs1Ability", DisciplineType::HACKER, true, 2000, 15, 671090653, 0, false, 0);
    addCore(97, "UILag1Ability", DisciplineType::HACKER, true, 2000, 15, 671088882, 671091266, false, 0);

    // Coder / Support Discipline
    addCore(77, "RestoreHealth1Ability", DisciplineType::CODER, true, 2000, 15, 671091234, 0x01000060, false, 0);
    addCore(80, "RestoreHealth2Ability", DisciplineType::CODER, true, 2000, 25, 671088928, 0x01000060, false, 0);
    addCore(234, "RestoreHealth3Ability", DisciplineType::CODER, true, 2000, 35, 671091238, 0x01000060, false, 0);
    addCore(46, "FastHealing1Ability", DisciplineType::CODER, true, 1000, 20, 671090613, 0x01000060, false, 0);
    addCore(169, "EmergencyRepairs1Ability", DisciplineType::CODER, true, 2000, 30, 671091173, 0x01000060, false, 0);
    addCore(56, "GroupRepairs1Ability", DisciplineType::CODER, true, 4000, 40, 671091250, 671091265, false, 0);
    addCore(50, "GroupRepairs2Ability", DisciplineType::CODER, true, 4000, 55, 671090992, 671091265, false, 0);
    addCore(39, "BolsterHealth1Ability", DisciplineType::CODER, true, 2000, 25, 671088751, 0, true, 300);
    addCore(375, "ReviveRSIAbility", DisciplineType::CODER, true, 6000, 60, 0x10314612, 0x01000060, false, 0);
    addCore(23, "DeflectCodeAbility", DisciplineType::CODER, false, 0, 0, 0x1D0A0028, 0, true, 0);
    addCore(20, "FortifySimulacra1Ability", DisciplineType::CODER, true, 2000, 20, 671090781, 0, false, 0);
    addCore(30, "RepairSimulacra1Ability", DisciplineType::CODER, true, 2000, 20, 671090845, 0, false, 0);

    // Martial Arts Disciplines
    // Self-Defense / Close Combat
    addCore(600, "CloseCombatTrainingAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 0, 0, 0, false, 0);
    addCore(137, "MartialArtsInitiateAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 10, 0, 0, false, 0);
    addCore(17, "SelfDefenseAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 0, 0, 0, false, 0);
    addCore(135, "MartialArtsAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0, false, 0);
    addCore(197, "Head Butt", DisciplineType::MARTIAL_ARTIST, true, 0, 10, 0, 0x280006DF, false, 0);
    addCore(198, "Cheap Shot", DisciplineType::MARTIAL_ARTIST, true, 0, 10, 0, 0x280006DF, false, 0);

    // Kung Fu (Wushu)
    addCore(133, "KungFuAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0x280006DF, false, 0);
    addCore(570, "KungFuCombatTacticsAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0x280006DF, false, 0);
    addCore(574, "KungFuDamageAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 20, 0, 0x280006DF, false, 0);
    addCore(8449, "KungFuMasterAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 25, 0, 0x280006DF, false, 0);
    addCore(8450, "KungfuMasteryAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 25, 0, 0x280006DF, false, 0);

    // Karate
    addCore(132, "KarateAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0x280006DF, false, 0);
    addCore(531, "KarateFocusAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 25, 0, 0x2800045A, false, 0);
    addCore(569, "KarateCombatTacticsAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0x280006DF, false, 0);
    addCore(573, "KarateDamageAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 20, 0, 0x280006DF, false, 0);
    addCore(8455, "KarateMasterAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 25, 0, 0x280006DF, false, 0);

    // Aikido
    addCore(101, "AikidoAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0x280006DF, false, 0);
    addCore(296, "AikidoSpinClayPigeonAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 25, 0, 0x28000432, false, 0);
    addCore(571, "AikidoCombatTacticsAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 15, 0, 0x280006DF, false, 0);
    addCore(572, "AikidoDamageAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 20, 0, 0x280006DF, false, 0);
    addCore(8461, "AikidoMasteryAbility", DisciplineType::MARTIAL_ARTIST, true, 0, 25, 0, 0x280006DF, false, 0);

    // Soldier / Gunner Discipline
    addCore(14, "PowerShotAbility", DisciplineType::GUNNER, true, 2000, 15, 671090470, 0x280006DF, false, 0);
    addCore(126, "PistolDisarmingShotAbility", DisciplineType::GUNNER, true, 1500, 20, 671090470, 0x280006DF, false, 0);
    addCore(128, "GunmanAbility", DisciplineType::GUNNER, false, 0, 0, 0, 0, true, 0);
    addCore(129, "HandgunsAbility", DisciplineType::GUNNER, true, 1000, 10, 671090470, 0x280006DF, false, 0);
    addCore(147, "RiflesAbility", DisciplineType::GUNNER, true, 2000, 20, 671090470, 0x280006DF, false, 0);
    addCore(154, "SoldierAbility", DisciplineType::GUNNER, false, 0, 0, 0, 0, true, 0);
    addCore(240, "GunslingerAbility", DisciplineType::GUNNER, false, 0, 0, 0, 0, true, 0);
    addCore(251, "RiflemanAbility", DisciplineType::GUNNER, false, 0, 0, 0, 0, true, 0);
    addCore(453, "RifleButtSmashAbility", DisciplineType::GUNNER, true, 1000, 15, 0, 0x280006DF, false, 0);
    addCore(499, "PistolPointBlankAbility", DisciplineType::GUNNER, true, 1000, 15, 671090470, 0x280006DF, false, 0);
    addCore(501, "PistolWhipAbility", DisciplineType::GUNNER, true, 1000, 10, 0, 0x280006DF, false, 0);
    addCore(505, "SniperShotAbility", DisciplineType::GUNNER, true, 4000, 35, 671090470, 0x280006DF, false, 0);

    // Spy Discipline
    addCore(146, "PoisonKnifeAbility", DisciplineType::SPY, true, 1500, 20, 671090469, 0x280006DF, false, 0);
    addCore(155, "SpyAbility", DisciplineType::SPY, false, 0, 0, 0, 0, true, 0);
    addCore(209, "StealthAbility", DisciplineType::SPY, true, 2000, 25, 671090262, 0, true, 60);
    addCore(283, "KnifeThrowerAbility", DisciplineType::SPY, true, 1500, 15, 671090469, 0x280006DF, false, 0);
    addCore(293, "StealthCountermeasuresAbility", DisciplineType::SPY, true, 2000, 20, 671091233, 0, true, 30);
    addCore(523, "StealthDamageAbility", DisciplineType::SPY, false, 0, 0, 0, 0, true, 0);
    addCore(1035, "StealthedCodeDamageAbility", DisciplineType::SPY, false, 0, 0, 0, 0, true, 0);

    // Core Weapon Templates for Soldier Dual-Wielding
    if (m_items.find(1001) == m_items.end())
    {
        ItemTemplate dualPistols;
        dualPistols.templateId = 1001;
        dualPistols.name = "Dual Beretta 92FS";
        dualPistols.type = ITEM_TYPE_WEAPON;
        dualPistols.isDualWield = true;
        dualPistols.minDamage = 15;
        dualPistols.maxDamage = 25;
        dualPistols.attackSpeed = 1.0f;
        m_items[1001] = dualPistols;
    }
}

bool DataLoader::LoadNPCs(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
    {
        WARNING_LOG(format("Could not open NPC data file: %1%") % filePath);
        return false;
    }

    static uint32 nextNpcId = 1;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        auto tokens = SplitCSVLine(line);
        if (tokens.size() >= 11)
        {
            try {
                NPCTemplate templ;
                templ.npcId = nextNpcId++;
                templ.district = tokens[1];
                templ.name = tokens[2];
                // Trim whitespace from name
                while (!templ.name.empty() && (templ.name.front() == ' ' || templ.name.front() == '\t')) templ.name.erase(0, 1);
                while (!templ.name.empty() && (templ.name.back() == ' ' || templ.name.back() == '\t')) templ.name.pop_back();

                std::string lvlStr = tokens[3];
                while (!lvlStr.empty() && (lvlStr.front() == ' ' || lvlStr.front() == '\t')) lvlStr.erase(0, 1);
                templ.level = (uint8)safe_stoul(lvlStr);
                if (templ.level == 0) templ.level = 1;

                templ.health = safe_stoul(tokens[4]);
                if (templ.health == 0) templ.health = templ.level * 60;

                templ.innerStrength = safe_stoul(tokens[5]);
                if (templ.innerStrength == 0) templ.innerStrength = templ.level * 40;

                templ.rsiHex = tokens[6];
                while (!templ.rsiHex.empty() && templ.rsiHex.front() == ' ') templ.rsiHex.erase(0, 1);
                while (!templ.rsiHex.empty() && templ.rsiHex.back() == ' ') templ.rsiHex.pop_back();

                // XYZ coords from indices 7,8,9
                templ.x = safe_stof(tokens[7]);
                templ.y = safe_stof(tokens[8]);
                templ.z = safe_stof(tokens[9]);
                templ.rot = safe_stof(tokens[10]);

                if (tokens.size() > 15) {
                    templ.weaponHex = tokens[15];
                    while (!templ.weaponHex.empty() && templ.weaponHex.front() == ' ') templ.weaponHex.erase(0, 1);
                    while (!templ.weaponHex.empty() && templ.weaponHex.back() == ' ') templ.weaponHex.pop_back();
                }

                // Determine authentic Faction based on NPC name
                std::string nameLower = templ.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                if (nameLower.find("agent") != std::string::npos ||
                    nameLower.find("machine") != std::string::npos ||
                    nameLower.find("sentinel") != std::string::npos ||
                    nameLower.find("zero one") != std::string::npos ||
                    nameLower.find("enforcer") != std::string::npos ||
                    nameLower.find("police") != std::string::npos ||
                    nameLower.find("cop") != std::string::npos ||
                    nameLower.find("swat") != std::string::npos ||
                    nameLower.find("guard") != std::string::npos)
                {
                    templ.faction = "Machines";
                    templ.isHostile = true;
                }
                else if (nameLower.find("blood") != std::string::npos ||
                         nameLower.find("vampire") != std::string::npos ||
                         nameLower.find("lupine") != std::string::npos ||
                         nameLower.find("blackwood") != std::string::npos ||
                         nameLower.find("madonna") != std::string::npos ||
                         nameLower.find("merovingian") != std::string::npos ||
                         nameLower.find("exile") != std::string::npos ||
                         nameLower.find("thug") != std::string::npos ||
                         nameLower.find("gangster") != std::string::npos ||
                         nameLower.find("corrupt") != std::string::npos)
                {
                    templ.faction = "Merovingian";
                    templ.isHostile = true;
                }
                else if (nameLower.find("zion") != std::string::npos ||
                         nameLower.find("redpill") != std::string::npos ||
                         nameLower.find("resistance") != std::string::npos ||
                         nameLower.find("operative") != std::string::npos ||
                         nameLower.find("captain") != std::string::npos)
                {
                    templ.faction = "Zion";
                    templ.isHostile = false;
                }
                else if (nameLower.find("oracle") != std::string::npos ||
                         nameLower.find("seraph") != std::string::npos ||
                         nameLower.find("sati") != std::string::npos)
                {
                    templ.faction = "Oracle";
                    templ.isHostile = false;
                }
                else
                {
                    templ.faction = "Civilian";
                    templ.isHostile = false;
                }
                
                m_npcs[templ.npcId] = templ;
            } catch (const std::exception& e) {
                WARNING_LOG(format("DataLoader: Failed to parse NPC line: %1%") % e.what());
            }
        }
    }
    INFO_LOG(format("DataLoader: Successfully loaded %1% authentic NPCs.") % m_npcs.size());
    return true;
}

const AbilityTemplate* DataLoader::GetAbilityTemplate(uint16 id) const
{
    auto it = m_abilities.find(id);
    if (it != m_abilities.end())
        return &it->second;
    return nullptr;
}

const ItemTemplate* DataLoader::GetItemTemplate(uint32 id) const
{
    auto it = m_items.find(id);
    if (it != m_items.end())
        return &it->second;
    return nullptr;
}

const NPCTemplate* DataLoader::GetNPCTemplate(uint32 id) const
{
    auto it = m_npcs.find(id);
    if (it != m_npcs.end())
        return &it->second;
    return nullptr;
}

bool DataLoader::LoadBlueprints(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
    {
        WARNING_LOG(format("Could not open blueprints data file: %1%") % filePath);
        return false;
    }

    std::string line;
    bool isFirstLine = true;
    while (std::getline(file, line))
    {
        if (isFirstLine) 
        {
            isFirstLine = false; // Skip header
            continue;
        }

        auto tokens = SplitCSVLine(line);
        if (tokens.size() >= 5)
        {
            try {
                uint32 id = safe_stoul(tokens[0]);
                
                CraftingBlueprint bp;
                bp.blueprintId = id;
                bp.resultingName = tokens[1];
                bp.infoCost = safe_stoul(tokens[2]);
                bp.craftTimeMs = safe_stoul(tokens[3]);
                
                // Parse component ids
                std::stringstream ss(tokens[4]);
                std::string comp;
                // Since SplitCSVLine might have used ';' or ',', wait, SplitCSVLine already splits by ';' or ','.
                // The fifth column tokens[4] is "1001;1002".
                // But wait! SplitCSVLine in DataLoader is:
                // char delimiter = line.find(';') != std::string::npos ? ';' : ',';
                // If the file uses ',' as main delimiter and ';' inside the column, SplitCSVLine will split by ','!
                // Wait, if it splits by ',', then tokens[4] = "1001;1002". We should split tokens[4] by ';'.
                while (std::getline(ss, comp, ';')) {
                    if (!comp.empty()) {
                        bp.requiredComponentIds.push_back(safe_stoul(comp));
                    }
                }
                
                m_blueprints[id] = bp;
            } catch (...) {
                // Ignore parsing errors for individual lines
            }
        }
    }

    return true;
}

bool DataLoader::LoadPropheticGlitchNodes(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
    {
        // Fallback default authentic nodes if CSV not found on disk
        PropheticGlitchNode g1{1, "Downtown", 1820.0f, 45.0f, -3150.0f, "KitchenResidue", "The Oracle's Kitchen - Cinnamon Fragrance and Golden Choice", false};
        PropheticGlitchNode g2{2, "Park East", -4750.0f, 82.0f, -2180.0f, "SatiSkyboxAnchor", "Sati's Dawn - Dynamic Golden Skybox Generator", false};
        PropheticGlitchNode g3{3, "Richland", 39216.0f, 500.0f, -21475.0f, "TemporalAssassinationResidue", "Morpheus Assassination Echo - Swarm of Digital Flies", false};
        PropheticGlitchNode g4{4, "International", -37444.0f, 500.0f, 23659.0f, "OligarchCrystallineAnchor", "Halborn Crystalline Subway Node - Pre-Source Conduit", false};
        PropheticGlitchNode g5{5, "The Slums", 99640.0f, 500.0f, 8350.0f, "TrainmanSmugglingAnchor", "Mobil Ave Purgatory Gate - Smuggler's Loop", false};
        m_glitchNodes[1] = g1;
        m_glitchNodes[2] = g2;
        m_glitchNodes[3] = g3;
        m_glitchNodes[4] = g4;
        m_glitchNodes[5] = g5;
        INFO_LOG(format("DataLoader: Initialized %1% default authentic Prophetic Glitch Nodes.") % m_glitchNodes.size());
        return true;
    }

    std::string line;
    bool isFirstLine = true;
    while (std::getline(file, line))
    {
        if (isFirstLine)
        {
            isFirstLine = false;
            continue;
        }
        auto tokens = SplitCSVLine(line);
        if (tokens.size() >= 6)
        {
            try {
                PropheticGlitchNode node;
                node.nodeId = safe_stoul(tokens[0]);
                node.district = tokens[1];
                node.posX = std::stof(tokens[2]);
                node.posY = std::stof(tokens[3]);
                node.posZ = std::stof(tokens[4]);
                node.anomalyType = tokens[5];
                if (tokens.size() >= 7) node.loreReward = tokens[6];
                node.isUnmasked = false;
                m_glitchNodes[node.nodeId] = node;
            } catch (...) {}
        }
    }
    INFO_LOG(format("DataLoader: Loaded %1% Prophetic Glitch Nodes from CSV.") % m_glitchNodes.size());
    return true;
}
