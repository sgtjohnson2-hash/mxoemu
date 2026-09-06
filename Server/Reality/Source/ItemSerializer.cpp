#include "ItemSerializer.h"
#include <sstream>

// Very simple JSON serializer for the mock implementation
// A robust JSON library like nlohmann/json or RapidJSON could be used later

std::string ItemSerializer::Serialize(std::shared_ptr<Item> item)
{
    if (!item) return "{}";

    std::stringstream ss;
    ss << "{";
    ss << "\"stackCount\":" << item->getStackCount() << ",";
    ss << "\"ammoCount\":" << item->getAmmoCount() << ",";
    ss << "\"durability\":" << item->getDurability() << ",";
    ss << "\"rarity\":" << (int)item->getRarity() << ",";
    ss << "\"isCorrupted\":" << (item->isCorrupted() ? "true" : "false");
    ss << "}";
    
    return ss.str();
}

void ItemSerializer::Deserialize(std::shared_ptr<Item> item, const std::string& metadata)
{
    if (!item || metadata.empty() || metadata == "{}") return;

    // Extremely basic manual parsing for our simple mock JSON
    // Format: {"stackCount":1,"ammoCount":0,"durability":100,"rarity":0,"isCorrupted":false}
    
    size_t pos = 0;
    
    pos = metadata.find("\"stackCount\":");
    if (pos != std::string::npos) {
        item->setStackCount(std::stoi(metadata.substr(pos + 13)));
    }
    
    pos = metadata.find("\"ammoCount\":");
    if (pos != std::string::npos) {
        item->setAmmoCount(std::stoi(metadata.substr(pos + 12)));
    }
    
    pos = metadata.find("\"durability\":");
    if (pos != std::string::npos) {
        item->setDurability(std::stof(metadata.substr(pos + 13)));
    }
    
    pos = metadata.find("\"rarity\":");
    if (pos != std::string::npos) {
        item->setRarity((ItemRarity)std::stoi(metadata.substr(pos + 9)));
    }
    
    pos = metadata.find("\"isCorrupted\":");
    if (pos != std::string::npos) {
        std::string val = metadata.substr(pos + 14, 4);
        item->setCorrupted(val == "true");
    }
}
