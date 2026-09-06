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
    LoadNPCs(directoryPath + "mob_parsed.csv");
    LoadBlueprints(directoryPath + "blueprints.csv");

    // Flyweight Personality Generation
    for (uint32 i = 0; i < 256; ++i) {
        m_personalities.push_back(BotPersonality::Generate(i + 1337));
    }

    INFO_LOG(format("DataLoader initialized. Loaded %1% items, %2% abilities, and %3% NPCs.") 
        % m_items.size() % m_abilities.size() % m_npcs.size());
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
    {
        WARNING_LOG(format("Could not open abilities data file: %1%") % filePath);
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
            // Identify Hacker Abilities based on name
            if (templ.name.find("Virus") != std::string::npos || 
                templ.name.find("Logic") != std::string::npos ||
                templ.name.find("Simulacra") != std::string::npos)
            {
                templ.discipline = DisciplineType::HACKER;
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
    return true;
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
