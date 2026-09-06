#include "SocketSystem.h"
#include "Item.h"
#include "PlayerObject.h"
#include "Log.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"
#include <boost/algorithm/string.hpp>

createFileSingleton(SocketSystem);

SocketSystem::SocketSystem()
{
}

SocketSystem::~SocketSystem()
{
}

void SocketSystem::initialize()
{
    INFO_LOG("SocketSystem Initialized.");
}

bool SocketSystem::ApplySocket(PlayerObject* player, uint64 gearUid, const CodeFragment& fragment)
{
    if (!player) return false;

    // We don't have a direct gearUid lookup in this mock, so we'll pretend we found the item.
    // In reality, this would query player->getInventory() for gearUid.
    
    // MOCK: Updating the database directly for demonstration
    std::string newSocketStr = (format("{\"type\":\"%1%\",\"value\":%2%}") % fragment.type % fragment.value).str();
    
    // Just a dirty append to a hypothetical JSON string in the DB.
    // We assume item_metadata structure looks like {"sockets": [...]}
    std::string sql = "UPDATE items SET metadata = CONCAT(SUBSTRING(metadata, 1, CHAR_LENGTH(metadata) - 1), ',\"sockets\":[', '" 
                      + newSocketStr + "', ']}') WHERE itemUid = ?0";

    PreparedStatement stmt(sql.c_str());
    stmt.SetUInt64(0, gearUid);
    sDatabase.ExecutePrepared(&stmt);

    INFO_LOG(format("SocketSystem: Player %1% socketed %2% (+%3%) into Item %4%") 
        % player->getHandle() % fragment.type % fragment.value % gearUid);
        
    return true;
}

std::vector<CodeFragment> SocketSystem::ParseSockets(const std::string& metadataJson)
{
    std::vector<CodeFragment> results;
    // Stub: string parse logic for 'type' and 'value' out of JSON arrays
    return results;
}

std::string SocketSystem::SerializeSockets(const std::vector<CodeFragment>& sockets, const std::string& existingJson)
{
    // Stub: reconstruct JSON array
    return existingJson;
}
