// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// ***************************************************************************

#ifndef MXOEMU_COMBATANIMATIONMATRIX_H
#define MXOEMU_COMBATANIMATIONMATRIX_H

#include "Common.h"

// Retail launch martial arts fighting styles (CombatEnums.cs / Client.dll)
enum class FightingStyle : uint8
{
    None = 0,       // Self-Defense / Street Brawling / Close Combat
    Aikido = 1,     // Aikido (Wrist locks, throws, momentum redirection)
    KungFu = 2,     // Wushu / Kung Fu (Wire-fu, mantis, tiger strikes)
    Karate = 3      // Karate (Ki blasts, leaping spin kicks, linear blocks)
};

enum class InterlockExchangeOutcome : uint8
{
    NormalHit = 0,
    StanceCrush = 1,        // e.g. Power crushes Speed
    FastInterrupt = 2,      // e.g. Speed interrupts Grab
    GuardBreak = 3,         // e.g. Grab breaks Guard or Power
    Blocked = 4,            // Defender successfully blocks
    Dodged = 5,             // Defender evades/dodges
    Clash = 6,              // Both combatants selected identical stance
    SpecialHit = 7,         // Ki blast, Headbutt, Cheap Shot
    Disarm = 8              // Martial artist disarms armed opponent
};

struct InterlockAnimPair
{
    uint16 attackerAnimId;
    uint16 defenderAnimId;
    uint32 hitFxId;
    float contactDelaySeconds;
};

class CombatAnimationMatrix
{
public:
    static InterlockAnimPair GetAnimationPair(
        FightingStyle attackerStyle,
        uint8 attackerTactic,
        FightingStyle defenderStyle,
        uint8 defenderTactic,
        InterlockExchangeOutcome outcome,
        uint16 moveId = 0
    );

    static uint16 GetDisarmAnimation(FightingStyle attackerStyle, uint32 weaponType);
};

#endif // MXOEMU_COMBATANIMATIONMATRIX_H
