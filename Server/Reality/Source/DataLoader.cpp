#include "DataLoader.h"
#include "Log.h"
#include <fstream>
#include <sstream>

createFileSingleton(DataLoader);

static unsigned long safe_stoul(const std::string& str) {
    if (str.empty()) return 0;
    try {
        return std::stoul(str);
    } catch (...) {
        return 0;
    }
}

DataLoader::DataLoader()
{
}

DataLoader::~DataLoader()
{
}

bool DataLoader::LoadAll(const std::string& directoryPath)
{
    // Try loading available data files.
    LoadClothing(directoryPath + "mxoClothing.csv");
    LoadAbilities(directoryPath + "abilityIDs.csv");
    LoadNPCs(directoryPath + "mob_parsed.csv");

    INFO_LOG(format("DataLoader initialized. Loaded %1% items, %2% abilities, and %3% NPCs.") 
        % m_items.size() % m_abilities.size() % m_npcs.size());
    return true;
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
            
            // Unused defaults
            templ.description = "Authentic Ability";
            templ.memoryCost = 10;
            templ.innerStrengthCost = 10;
            templ.cooldown = 5000;
            templ.maxLevel = 1;
            templ.discipline = DisciplineType::NONE;
            
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
        auto tokens = SplitCSVLine(line);
        if (tokens.size() >= 11)
        {
            try {
                NPCTemplate templ;
                templ.npcId = nextNpcId++;
                templ.district = tokens[1];
                templ.name = tokens[2];
                templ.level = safe_stoul(tokens[3]);
                templ.health = safe_stoul(tokens[4]);
                templ.innerStrength = safe_stoul(tokens[5]);
                // XYZ coords from indices 7,8,9
                templ.x = std::stof(tokens[7]);
                templ.y = std::stof(tokens[8]);
                templ.z = std::stof(tokens[9]);
                templ.rot = std::stof(tokens[10]);
                
                m_npcs[templ.npcId] = templ;
            } catch (const std::exception& e) {
                WARNING_LOG(format("DataLoader: Failed to parse NPC line: %1%") % e.what());
            }
        }
    }
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
