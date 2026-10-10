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

bool SocketSystem::ApplySocket(PlayerObject* player, uint64 gearUid, const SocketFragment& fragment)
{
    if (!player) return false;

    // Fetch existing metadata from inventory table
    std::string currentJson = "{}";
    uint32 targetInvId = 0;
    
    if (Database_Main != nullptr) {
        // Check if gearUid is a slot (1..30)
        if (gearUid <= 30) {
            PreparedStatement slotStmt("SELECT invId, item_metadata FROM inventory WHERE charId = ?0 AND slot = ?1 LIMIT 1");
            slotStmt.SetUInt64(0, player->getCharacterUID());
            slotStmt.SetUInt32(1, (uint32)gearUid);
            scoped_ptr<QueryResult> res(sDatabase.QueryPrepared(&slotStmt));
            if (res && res->GetRowCount() > 0) {
                Field* fields = res->Fetch();
                targetInvId = fields[0].GetUInt32();
                currentJson = fields[1].GetString();
            }
        } else {
            PreparedStatement idStmt("SELECT invId, item_metadata FROM inventory WHERE (invId = ?0 OR goid = ?1) AND charId = ?2 LIMIT 1");
            idStmt.SetUInt64(0, gearUid);
            idStmt.SetUInt64(1, gearUid);
            idStmt.SetUInt64(2, player->getCharacterUID());
            scoped_ptr<QueryResult> res(sDatabase.QueryPrepared(&idStmt));
            if (res && res->GetRowCount() > 0) {
                Field* fields = res->Fetch();
                targetInvId = fields[0].GetUInt32();
                currentJson = fields[1].GetString();
            }
        }
    }

    if (targetInvId == 0) {
        targetInvId = (uint32)gearUid;
    }

    auto sockets = ParseSockets(currentJson);
    sockets.push_back(fragment);
    std::string updatedJson = SerializeSockets(sockets, currentJson);

    if (Database_Main != nullptr) {
        PreparedStatement updateStmt("UPDATE inventory SET item_metadata = ?0 WHERE invId = ?1");
        updateStmt.SetString(0, updatedJson);
        updateStmt.SetUInt32(1, targetInvId);
        sDatabase.ExecutePrepared(&updateStmt);
    }

    INFO_LOG(format("SocketSystem: Player %1% socketed %2% (+%3%) into Item (invId %4%)") 
        % player->getHandle() % fragment.type % fragment.value % targetInvId);
        
    return true;
}

std::vector<SocketFragment> SocketSystem::ParseSockets(const std::string& metadataJson)
{
    std::vector<SocketFragment> results;
    size_t pos = metadataJson.find("\"sockets\"");
    if (pos == std::string::npos) return results;
    size_t start = metadataJson.find('[', pos);
    size_t end = metadataJson.find(']', start);
    if (start == std::string::npos || end == std::string::npos) return results;
    
    std::string arr = metadataJson.substr(start + 1, end - start - 1);
    size_t objStart = 0;
    while ((objStart = arr.find('{', objStart)) != std::string::npos) {
        size_t objEnd = arr.find('}', objStart);
        if (objEnd == std::string::npos) break;
        std::string obj = arr.substr(objStart + 1, objEnd - objStart - 1);
        
        // Find "type": "..."
        size_t typePos = obj.find("\"type\"");
        std::string type;
        int value = 0;
        if (typePos != std::string::npos) {
            size_t colon = obj.find(':', typePos);
            size_t q1 = obj.find('"', colon);
            size_t q2 = (q1 != std::string::npos) ? obj.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos) {
                type = obj.substr(q1 + 1, q2 - q1 - 1);
            }
        }
        
        // Find "value": ...
        size_t valPos = obj.find("\"value\"");
        if (valPos != std::string::npos) {
            size_t colon = obj.find(':', valPos);
            if (colon != std::string::npos) {
                value = atoi(obj.c_str() + colon + 1);
            }
        }
        
        if (!type.empty()) {
            results.push_back({type, value});
        }
        objStart = objEnd + 1;
    }
    return results;
}

std::string SocketSystem::SerializeSockets(const std::vector<SocketFragment>& sockets, const std::string& existingJson)
{
    std::string sockArr = "\"sockets\":[";
    for (size_t i = 0; i < sockets.size(); ++i) {
        if (i > 0) sockArr += ",";
        sockArr += (format("{\"type\":\"%1%\",\"value\":%2%}") % sockets[i].type % sockets[i].value).str();
    }
    sockArr += "]";

    // If existingJson is empty or "{}", wrap it
    if (existingJson.empty() || existingJson == "{}") {
        return "{" + sockArr + "}";
    }

    // If existingJson already has "sockets", replace that section
    size_t pos = existingJson.find("\"sockets\"");
    if (pos != std::string::npos) {
        size_t start = existingJson.find('[', pos);
        size_t end = (start != std::string::npos) ? existingJson.find(']', start) : std::string::npos;
        if (start != std::string::npos && end != std::string::npos) {
            std::string before = existingJson.substr(0, pos);
            std::string after = existingJson.substr(end + 1);
            return before + sockArr + after;
        }
    }

    // Otherwise insert before the closing brace '}'
    size_t lastBrace = existingJson.rfind('}');
    if (lastBrace != std::string::npos) {
        std::string before = existingJson.substr(0, lastBrace);
        std::string prefix = (before.find_first_not_of("{ \t\r\n") != std::string::npos) ? "," : "";
        return before + prefix + sockArr + "}";
    }
    return "{" + sockArr + "}";
}
