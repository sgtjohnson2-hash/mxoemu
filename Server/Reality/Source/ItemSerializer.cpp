#include "ItemSerializer.h"
#include <sstream>

// Very simple JSON serializer for the mock implementation
// A robust JSON library like nlohmann/json or RapidJSON could be used later

std::string ItemSerializer::Serialize(std::shared_ptr<Item> item)
{
    if (!item) return "{}";

    std::stringstream ss;
    ss << "{";
    ss << "\"templateId\":" << item->getTemplateId() << ",";
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

    // Manual parsing for key:value pairs
    size_t pos = 0;
    
    pos = metadata.find("\"templateId\":");
    if (pos != std::string::npos) {
        try {
            item->setTemplateId((uint32)std::stoul(metadata.substr(pos + 13)));
        } catch (...) {}
    }

    pos = metadata.find("\"stackCount\":");
    if (pos != std::string::npos) {
        try {
            item->setStackCount((uint16)std::stoi(metadata.substr(pos + 13)));
        } catch (...) {}
    }
    
    pos = metadata.find("\"ammoCount\":");
    if (pos != std::string::npos) {
        try {
            item->setAmmoCount((uint16)std::stoi(metadata.substr(pos + 12)));
        } catch (...) {}
    }
    
    pos = metadata.find("\"durability\":");
    if (pos != std::string::npos) {
        try {
            item->setDurability(std::stof(metadata.substr(pos + 13)));
        } catch (...) {}
    }
    
    pos = metadata.find("\"rarity\":");
    if (pos != std::string::npos) {
        try {
            item->setRarity((ItemRarity)std::stoi(metadata.substr(pos + 9)));
        } catch (...) {}
    }
    
    pos = metadata.find("\"isCorrupted\":");
    if (pos != std::string::npos) {
        std::string val = metadata.substr(pos + 14, 4);
        item->setCorrupted(val == "true");
    }
}
