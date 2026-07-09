// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
// Copyright (C) 2006-2010 Rajko Stojadinovic
// http://mxoemu.info
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ---------------------------------------------------------------------------
//
// ***************************************************************************

#ifndef MXOSIM_GOATTRIBUTES_H
#define MXOSIM_GOATTRIBUTES_H

#include "Common.h"
#include "ByteBuffer.h"

// Attribute-update block builder for the MxO view protocol.
//
// View attribute updates encode which attributes changed as a sequence of
// 7-bit group headers: bit j of group g marks attribute (g*7+j), bit 7 marks
// "another group follows". Each group header is immediately followed by the
// values of its flagged attributes in ascending index order. Other-view
// updates are prefixed with a count of flagged attributes, self-view updates
// are not. (Layouts recovered from the CR2 client, cross-checked with HDS.)
class AttributeUpdateBlock
{
public:
	AttributeUpdateBlock() {}

	void addAttribute(uint8 index, const ByteBuffer &value)
	{
		m_attribs[index] = value;
	}
	void addByte(uint8 index, uint8 value)
	{
		ByteBuffer buf; buf << value; addAttribute(index,buf);
	}
	void addUInt16(uint8 index, uint16 value)
	{
		ByteBuffer buf; buf << value; addAttribute(index,buf);
	}
	void addUInt32(uint8 index, uint32 value)
	{
		ByteBuffer buf; buf << value; addAttribute(index,buf);
	}

	//serialize: [count?] group0 [values...] group1 [values...] ...
	ByteBuffer toBuf(bool includeCount) const
	{
		ByteBuffer result;
		if (m_attribs.empty())
			return result;

		uint8 lastGroup = m_attribs.rbegin()->first / 7;

		if (includeCount)
			result << uint8(m_attribs.size());

		for (uint8 g=0; g<=lastGroup; g++)
		{
			uint8 header = 0;
			for (uint8 j=0; j<7; j++)
			{
				if (m_attribs.count(g*7+j))
					header |= (1 << j);
			}
			if (g < lastGroup)
				header |= 0x80; //more groups follow

			result << uint8(header);

			for (uint8 j=0; j<7; j++)
			{
				map<uint8,ByteBuffer>::const_iterator it = m_attribs.find(g*7+j);
				if (it != m_attribs.end())
					result.append(it->second.contents(),it->second.size());
			}
		}
		return result;
	}
private:
	map<uint8,ByteBuffer> m_attribs; //index -> raw value bytes, kept sorted
};

// PlayerCharacter (GO 0x000C) attribute indexes for OTHER-view updates
// (36 update attributes, from the CR2 Object12 update mapping)
namespace PlayerAttrOther
{
	enum
	{
		UseRSIDescription	= 0,
		TitleAbility		= 1,
		RepelDistance		= 2,
		RSIDescription		= 3,
		OrganizationID		= 4,
		MovementScale		= 5,
		MissionTeamID		= 6,
		MissionKey			= 7,
		MaxHealth			= 8,
		Level				= 9,
		JumpPeakHeight		= 10,
		JumpFlags			= 11,
		JumpEndTime			= 12,
		JumpDestination		= 13,
		IsDuelDeath			= 14,
		IsDead				= 15,
		InteractionFlags	= 16,
		HeavyLuggableID		= 17,
		Health				= 18,
		FollowingPath		= 19,
		FactionID			= 20,
		EvadeShieldHealth	= 21,
		EquippedItemID		= 22,
		EffectID			= 23,
		EffectCounter		= 24,
		EffectCommand		= 25,
		DuelID				= 26,
		DissemblingType		= 27,
		Description			= 28,
		CrewID				= 29,
		ConditionStateFlags	= 30,
		CombatantMode		= 31,
		CancelAbilityCounter= 32,
		CancelAbility		= 33,
		AbandonedState		= 34,
		AFK					= 35,
	};
}

// PlayerCharacter attribute indexes for SELF-view updates
// (48 self-view attributes; self view is always view id 2 on the client)
namespace PlayerAttrSelf
{
	enum
	{
		UseRSIDescription	= 0,
		TitleAbility		= 1,
		SelectionRangeDebuff= 2,
		RippleMagnitude		= 3,
		RepelDistance		= 4,
		RealLastName		= 5,
		RealFirstName		= 6,
		RSIDescription		= 7,
		OrganizationID		= 8,
		NoiseLevel			= 9,
		MovementScale		= 10,
		MissionTypeFlags	= 11,
		MissionTeamID		= 12,
		MissionKey			= 13,
		MaxHealth			= 14,
		Level				= 15,
		JumpPeakHeight		= 16,
		JumpFlags			= 17,
		JumpEndTime			= 18,
		JumpDestination		= 19,
		IsDuelDeath			= 20,
		IsDead				= 21,
		InteractionFlags	= 22,
		InnerStrengthMax	= 23,
		InnerStrengthCommitted = 24,
		InnerStrengthAvailable = 25,
		HeavyLuggableID		= 26,
		Health				= 27,
		FollowingPath		= 28,
		FactionID			= 29,
		EvadeShieldHealth	= 30,
		EquippedItemID		= 31,
		EffectID			= 32,
		EffectCounter		= 33,
		EffectCommand		= 34,
		DuelID				= 35,
		DissemblingType		= 36,
		Description			= 37,
		DeathPenalty		= 38,
		CurCombatExclusiveAbility = 39,
		CrewID				= 40,
		ConquestPoints		= 41,
		ConditionStateFlags	= 42,
		CombatantMode		= 43,
		CancelAbilityCounter= 44,
		CancelAbility		= 45,
		AbandonedState		= 46,
		AFK					= 47,
	};
}

//the client reserves view 1 for the object manager and view 2 for one's own character
static const uint16 VIEWID_OBJECTMANAGER = 1;
static const uint16 VIEWID_SELF = 2;

//ILCombatHandler game object (GOID 55): creation attributes are
//Position(24B double vec, idx0), HalfExtents(12B, idx1), StartTime(float, idx2)
static const uint16 GOID_ILCOMBATHANDLER = 55;
static const uint16 GOID_PLAYERCHARACTER = 0x000C;

#endif
