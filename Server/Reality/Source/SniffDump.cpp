#include "Common.h"
#include "SniffDump.h"
#include "Util.h"
#include <typeinfo>

extern bool g_sniffPackets;

// Try to serialize `msg` as type T. Returns true if msg IS a T (whether or not
// serialization succeeded), so the caller's short-circuit chain stops probing.
template<typename T>
static bool tryDump(const msgBaseClassPtr& msg, const char* kind, std::ofstream& out)
{
	std::shared_ptr<T> p = std::dynamic_pointer_cast<T>(msg);
	if (!p)
		return false;
	try
	{
		const ByteBuffer& b = p->toBuf();
		if (b.size() > 0)
		{
			// Bin2Hex defaults to space+newline separators; flags=0 gives one
			// continuous hex run so each dump line stays valid JSON.
			out << "{\"direction\": \"out\", \"client\": \"bot\", \"kind\": \"" << kind
				<< "\", \"msgtype\": \"" << typeid(*msg).name()
				<< "\", \"hex\": \"" << Bin2Hex(b, 0) << "\"}\n";
		}
	}
	catch (...) { /* PacketNoLongerValid and friends: skip this one */ }
	return true;
}

void SniffTrySerialize(const msgBaseClassPtr& msg, const char* kind, std::ofstream& out)
{
	if (!msg)
		return;

	// Whitelist the combat / interlock / ability message families only. The ||
	// chain stops at the first type that matches, so exactly one branch runs.
	tryDump<HealthUpdateMsg>(msg, kind, out) ||
	tryDump<CombatHitFxMsg>(msg, kind, out) ||
	tryDump<CombatantModeMsg>(msg, kind, out) ||
	tryDump<SelfVitalsMsg>(msg, kind, out) ||
	tryDump<SelfHitFxMsg>(msg, kind, out) ||
	tryDump<SelfCombatantModeMsg>(msg, kind, out) ||
	tryDump<CastBarMsg>(msg, kind, out) ||
	tryDump<AbilityLoadRspMsg>(msg, kind, out) ||
	tryDump<AbilityUnloadRspMsg>(msg, kind, out) ||
	tryDump<DeleteViewMsg>(msg, kind, out) ||
	tryDump<SpawnILCombatHandlerMsg>(msg, kind, out) ||
	tryDump<InterlockInitMsg>(msg, kind, out);
}
