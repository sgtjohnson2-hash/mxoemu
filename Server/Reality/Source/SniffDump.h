#pragma once
#include "MessageTypes.h"
#include <fstream>

// Serialize `msg` to `out` as one JSON line IF it is a whitelisted combat /
// interlock / ability message. Non-combat traffic (position, spawn, chat,
// emote, RSI/appearance) is skipped on purpose: those message types deref
// per-recipient view state that bots do not register, so calling toBuf() on
// them raises a Win32 access violation (not a C++ exception, so catch(...)
// cannot save us). Whitelisting means we only ever call toBuf() on the
// combat families whose encoders are known safe (StaticMsg self-view packets
// carry a prebuilt buffer; the ObjectUpdateMsg combat packets guard their
// object/view lookups and throw PacketNoLongerValid, which we catch).
void SniffTrySerialize(const msgBaseClassPtr& msg, const char* kind, std::ofstream& out);
