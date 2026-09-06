#ifndef MXOEMU_SOCKET_SYSTEM_H
#define MXOEMU_SOCKET_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>

class PlayerObject;
class Item; // Assuming an Item class exists in the codebase

// Structure to define what a code fragment modifies
struct CodeFragment {
    std::string type;   // e.g., "damage", "defense", "health"
    int value;
};

class SocketSystem : public Singleton<SocketSystem>{
public:
    SocketSystem();
    ~SocketSystem();

    void initialize();

    // Attempts to socket a code fragment into a piece of gear.
    // In a real implementation this would consume the fragment item.
    // Returns true if successful, false otherwise.
    bool ApplySocket(PlayerObject* player, uint64 gearUid, const CodeFragment& fragment);

private:
    // Helper to parse existing sockets from the gear's m_metadataJSON
    std::vector<CodeFragment> ParseSockets(const std::string& metadataJson);
    
    // Helper to serialize sockets back to JSON
    std::string SerializeSockets(const std::vector<CodeFragment>& sockets, const std::string& existingJson);
};

#define sSocketSystem Singleton<SocketSystem>::getSingleton()

#endif // MXOEMU_SOCKET_SYSTEM_H
