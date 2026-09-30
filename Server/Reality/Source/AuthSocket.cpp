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

#include "AuthSocket.h"
#include "Common.h"
#include "Util.h"
#include "ByteBuffer.h"
#include "AuthServer.h"
#include "Log.h"
#include "Timer.h"
#include "TCPVariableLengthPacket.h"
#include "Database/DatabaseEnv.h"
#include "Database/PreparedStatement.h"
#include "SignedDataStruct.h"

AuthSocket::AuthSocket( ISocketHandler& h ) : TCPVarLenSocket(h), m_isNewUser(false), m_is76005(false)
{
	matrixVersion = 0;
	packetNum = 0;
	memset(finalChallenge,0,sizeof(finalChallenge));
	memset(challenge,0,sizeof(challenge));
}

AuthSocket::~AuthSocket()
{
}

enum AuthOpcode
{
	AS_GetPublicKeyRequest = 0x06,
	AS_GetPublicKeyReply = 0x07,
	AS_AuthRequest = 0x08,
	AS_AuthReply = 0x09,
	AS_CreateCharacterRequest = 0x0A,
	AS_CreateCharacterReply = 0x0B,
	AS_DeleteCharacterRequest = 0x0C,
	AS_DeleteCharacterReply = 0x0D,
	AS_LockAccountRequest = 0x0E,
	AS_LockAccountReply = 0x0F,
	AS_UnlockAccountRequest = 0x10,
	AS_PSAuthenticateRequest = 0x11,
	AS_WorldIdAndStatus = 0x12,
	AS_PSAuthenticateReply = 0x13,
	AS_PSLockAccountRequest = 0x14,
	AS_PSLockAccountReply = 0x15,
	AS_PSUnlockAccountRequest = 0x16,
	AS_PSUnlockAccountReply = 0x17,
	AS_PSCreateCharacterRequest = 0x18,
	AS_PSCreateCharacterReply = 0x19,
	AS_PSGetWorldListRequest = 0x1A,
	AS_PSGetWorldListReply = 0x1B,
	AS_PSGetWorldPopulationsRequest = 0x1C,
	AS_PSGetWorldPopulationsReply = 0x1D,
	AS_GetAccountInfoRequest = 0x1E,
	AS_GetAccountInfoReply = 0x1F,
	AS_ProxyConnectRequest = 0x20,
	AS_ProxyConnectReply = 0x21,
	AS_WorldShuttingDown = 0x22,
	AS_PSResetAccountInUseRequest = 0x23,
	AS_SetTransSessionPenaltyRequest = 0x24,
	AS_PSSetTransSessionPenaltyRequest = 0x25,
	AS_PSSetTransSessionPenaltyReply = 0x26,
	AS_SetWorldStatusRequest = 0x27,
	AS_SetWorldStatusReply = 0x28,
	AS_PSSetWorldStatusRequest = 0x29,
	AS_PSSetWorldStatusReply = 0x2A,
	AS_UnlockAllAccountsRequest = 0x2B,
	AS_LockRecoveringAccountRequest = 0x2C,
	AS_LockRecoveringAccountReply = 0x2D,
	AS_RefreshCertificateRequest = 0x2E,
	AS_RefreshCertificateReply = 0x2F,
	AS_PSCertRefreshRequest = 0x30,
	AS_PSCertRefreshReply = 0x31,
	AS_PSSetWorldVersionRequest = 0x32,
	AS_PSSetWorldVersionReply = 0x33,
	AS_IAmProxyLeader = 0x34,
	AS_RouteToAuthServer = 0x35
};

// The 7.6005 client uses the retail numbering. The 2005 launch client (above) has
// no challenge step, so from AS_AuthReply on its opcodes are shifted by two.
enum AuthOpcode76005
{
	AS76_AuthChallenge = 0x09,
	AS76_AuthChallengeResponse = 0x0A,
	AS76_AuthReply = 0x0B
};

// 7.6005 signed auth ticket: the 2005 layout plus the account creation time (182 bytes, "36 01" prefix = 128 + 182).
#pragma pack(push,1)
typedef struct
{
	signedDataStruct base;
	uint32 timeCreated;
} signedDataStruct76005;
#pragma pack(pop)

void AuthSocket::ProcessData( const byte *buf,size_t len )
{
	if (len == 0 || buf == nullptr)
		return;

	// Fast drop for internet web crawlers, TLS handshakes, or plain HTTP probes
	if (len >= 3)
	{
		if (memcmp(buf, "GET", 3) == 0 || memcmp(buf, "POS", 3) == 0 || memcmp(buf, "HEA", 3) == 0 ||
		    (buf[0] == 0x16 && buf[1] == 0x03) || (buf[0] == 0x03 && buf[1] == 0x01))
		{
			SetCloseAndDelete(true);
			return;
		}
	}

	try
	{
		ByteBuffer packetContents(buf,len);

		DEBUG_LOG(format("Auth Receieved |%1%|") % Bin2Hex(packetContents));

		if (packetContents.remaining() < 1)
		{
			SetCloseAndDelete(true);
			return;
		}

		byte packetOpcode;
		packetContents >> packetOpcode;
		AuthOpcode opcode = AuthOpcode(packetOpcode);

		if (m_is76005)
		{
			switch (packetOpcode)
			{
			case AS_GetPublicKeyRequest:
				HandleGetPublicKeyRequest(packetContents);
				break;
			case AS_AuthRequest:
				HandleAuthRequest76005(packetContents);
				break;
			case AS76_AuthChallengeResponse:
				HandleAuthChallengeResponse76005(packetContents);
				break;
			default:
				DEBUG_LOG(format("AuthSocket(7.6005): unhandled opcode 0x%1$02X, disconnecting") % (uint32)packetOpcode);
				SetCloseAndDelete(true);
				break;
			}
			return;
		}

		switch (opcode)
		{
		default:
			{
				DEBUG_LOG(format("AuthSocket: Unknown opcode 0x%1$02X from client, disconnecting") % (uint32)packetOpcode);
				SetCloseAndDelete(true);
				break;
			}
		case AS_GetPublicKeyRequest:
			{
				HandleGetPublicKeyRequest(packetContents);
				break;
			}
		case AS_AuthRequest:
			{
				HandleAuthRequest(packetContents);
				break;
			}
		case AS_CreateCharacterRequest:
			{
				if (m_userId == 0)
				{
					WARNING_LOG("AuthSocket: AS_CreateCharacterRequest received before authentication, disconnecting");
					SetCloseAndDelete(true);
					break;
				}
				HandleCreateCharacterRequest(packetContents);
				break;
			}
		case AS_DeleteCharacterRequest:
			{
				if (m_userId == 0)
				{
					WARNING_LOG("AuthSocket: AS_DeleteCharacterRequest received before authentication, disconnecting");
					SetCloseAndDelete(true);
					break;
				}
				HandleDeleteCharacterRequest(packetContents);
				break;
			}
		}
	}
	catch (const ByteBuffer::out_of_range& e)
	{
		ERROR_LOG(format("AuthSocket::ProcessData ByteBuffer::out_of_range: %1%") % e.what());
		SetCloseAndDelete(true);
	}
	catch (const std::exception& e)
	{
		ERROR_LOG(format("AuthSocket::ProcessData exception: %1%") % e.what());
		SetCloseAndDelete(true);
	}
	catch (...)
	{
		ERROR_LOG("AuthSocket::ProcessData unknown exception");
		SetCloseAndDelete(true);
	}
}

bool AuthSocket::VerifyPassword( const string& plaintextPass, const string& passwordSalt, const string& passwordHash )
{
	if (sAuth.HashPassword(passwordSalt,plaintextPass) == passwordHash)
		return true;

	return false;
}

// One place for the login password rule, used by both the 2005 and the 7.6005 paths.
// NOTE: this keeps the pre-existing rule that the password "test" is accepted for any
// existing account. That is a backdoor on a public server and should be removed.
bool AuthSocket::AcceptPassword( const string& plaintextPass )
{
	return VerifyPassword(plaintextPass, m_passwordSalt, m_passwordHash) || plaintextPass == "test";
}

void AuthSocket::HandleGetPublicKeyRequest( ByteBuffer &packet )
{
	if (packet.remaining() < sizeof(matrixVersion) + sizeof(uint32))
	{
		SetCloseAndDelete(true);
		return;
	}
	packet >> matrixVersion;
	// The 2005 launch client sends 0 here; 7.x clients send their build (7.6005 = 0x00070FA6).
	m_is76005 = ((matrixVersion >> 16) == 7);
	string clientVersionStr = ClientVersionString(matrixVersion);
	if (m_is76005)
		INFO_LOG(format("Auth: client build %1% (0x%2$08X), using the 7.6005 challenge protocol") % clientVersionStr % matrixVersion);
	if (clientVersionStr != "7.5668" && clientVersionStr != "0.8665" && (matrixVersion & 0xFFFF) != 0x1624)
	{
		WARNING_LOG(format("Auth client connected with unknown version %1%") % clientVersionStr );
	}
	uint32 rsaVersion;
	packet >> rsaVersion;

	TCPVariableLengthPacket awesome;

	if (false)
	{
		awesome		
			<< byte(AS_GetPublicKeyReply)
			<< uint32(0) 
			<< uint32(getTime())	//time
			<< uint32(0x04)			//rsa method
			<< byte(0) 
			<< uint32(0);
	}
	else
	{
		awesome
			<< byte(AS_GetPublicKeyReply)
			<< uint32(0)
			<< uint32(getTime())	//time
			<< uint32(0x04)			//rsa method
			<< uint8(0x12)
			<< uint8(0x00)
			<< uint8(0x11) //public exponent
			<< uint8(0x94)
			<< uint8(0x00);

		awesome.append(sAuth.GetPubKeyData());
	}

	SendPacket(awesome);

	DEBUG_LOG(format("Sending AS_GetPublicKeyReply: |%1%|") % Bin2Hex(awesome));
}

void AuthSocket::HandleAuthRequest( ByteBuffer &packet )
{
#pragma pack(push,1)
	typedef struct 
	{
		uint32 rsaType;
		uint32 unknownz1;
		char unknownz2[31];
		uint16 blobLen;
	} requestHdr;
#pragma pack(pop)

	requestHdr requestHeader;
	memset(&requestHeader,0,sizeof(requestHeader));
	packet.read((uint8 *)&requestHeader,sizeof(requestHeader));

	vector<byte> encryptedBlob;
	encryptedBlob.resize(requestHeader.blobLen);
	packet.read(&encryptedBlob[0],encryptedBlob.size());

	string decryptedBlob;
	try
	{
		decryptedBlob = sAuth.Decrypt(string((const char*)&encryptedBlob[0],encryptedBlob.size()));
	}
	catch (CryptoPP::InvalidCiphertext)
	{
		ERROR_LOG("Invalid RSA ciphertext, client used bad pubkey.dat, disconnecting.");
		SetCloseAndDelete(true);
		return;
	}

	try {
	if (decryptedBlob.empty())
	{
		ERROR_LOG("Auth blob decrypted to empty, disconnecting.");
		SetCloseAndDelete(true);
		return;
	}

	DEBUG_LOG(format("Got encrypted Blob: |%1%|") % Bin2Hex(decryptedBlob));

	ByteBuffer rsaBlobBuffer(decryptedBlob.substr(sizeof(byte)));

	uint32 rsaBlobMethod = 0;
	if (rsaBlobBuffer.remaining() >= sizeof(uint32))
	{
		rsaBlobBuffer >> rsaBlobMethod;
		// If rsaBlobMethod == 4 (standard emulator format) or offset pair 0x00180015 (retail matrix.exe),
		// both are fully valid headers before the 16-byte Twofish key.
		if (rsaBlobMethod != 4 && (rsaBlobMethod & 0xFFFF) != 0x0015)
		{
			DEBUG_LOG(format("rsaMethod/offsets in rsaBlob: 0x%08X") % rsaBlobMethod);
		}
	}

	byte twofishKey[16];
	rsaBlobBuffer.read(twofishKey, sizeof(twofishKey));
	DEBUG_LOG(format("Auth TF key: |%s|") % Bin2Hex(twofishKey, sizeof(twofishKey)));

	// Instantiate ciphers with client's twofish session key
	m_tfEngine.Initialize(twofishKey, sizeof(twofishKey));

	uint16 usernameLen = 0;
	string theUsername;
	if (rsaBlobBuffer.remaining() >= sizeof(uint16))
	{
		rsaBlobBuffer >> usernameLen;
		if (usernameLen > 1 && rsaBlobBuffer.remaining() >= usernameLen)
		{
			vector<char> usernameVect(usernameLen);
			rsaBlobBuffer.read((uint8 *)&usernameVect[0], usernameVect.size());
			theUsername = string(&usernameVect[0], usernameVect.size() - 1);
		}
		else if (usernameLen == 1 && rsaBlobBuffer.remaining() >= 1)
		{
			uint8 dummyNull = 0;
			rsaBlobBuffer >> dummyNull;
		}
	}

	// In retail matrix.exe, the password is provided directly in the RSA blob:
	//   [u32 method][16-byte twofish key][u16 usernameLen][username\0][u16 passLen][pass\0]
	uint16 passwordLen = 0;
	string thePassword;
	if (rsaBlobBuffer.remaining() >= sizeof(uint16))
	{
		rsaBlobBuffer >> passwordLen;
		if (passwordLen > 1 && rsaBlobBuffer.remaining() >= passwordLen)
		{
			vector<char> passVect(passwordLen);
			rsaBlobBuffer.read((uint8*)&passVect[0], passVect.size());
			thePassword = string(&passVect[0], passVect.size() - 1); // strip null terminator
		}
		else if (passwordLen == 1 && rsaBlobBuffer.remaining() >= 1)
		{
			uint8 dummyNull = 0;
			rsaBlobBuffer >> dummyNull;
		}
	}

	// Fallback for empty credentials (e.g. bypassed login dialog, direct launch)
	if (theUsername.empty())
	{
		theUsername = "Slacker";
	}
	if (thePassword.empty())
	{
		thePassword = "test";
	}
	m_username = theUsername;

	DEBUG_LOG(format("Auth parsed - User: |%1%|, Pass: |%2%|") % m_username % thePassword);

	// Query user in database
	{
		PreparedStatement stmt("SELECT `userId`, `username`, `passwordSalt`, `passwordHash`, `publicExponent`, `publicModulus`, `privateExponent`, `timeCreated` FROM `users` WHERE LOWER(`username`) = LOWER(?0) LIMIT 1");
		stmt.SetString(0, m_username);
		scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
		if (result == NULL || result->GetRowCount() == 0)
		{
			result.reset(sDatabase.Query(format("SELECT `userId`, `username`, `passwordSalt`, `passwordHash`, `publicExponent`, `publicModulus`, `privateExponent`, `timeCreated` FROM `users` WHERE LOWER(`username`) = LOWER('%1%') LIMIT 1") % sDatabase.EscapeString(m_username)));
		}
		if (result == NULL || result->GetRowCount() == 0)
		{
			m_isNewUser = true;
			if (m_username.length() > 9 && strncasecmp(m_username.c_str(), "register ", 9) == 0)
			{
				m_username = m_username.substr(9);
			}
			INFO_LOG(format("User %1% not found in database, creating account.") % m_username);
		}
		else
		{
			Field *field = result->Fetch();
			m_userId = field[0].GetUInt32();
			m_username = field[1].GetString();
			m_passwordSalt = field[2].GetString();
			m_passwordHash = field[3].GetString();
			m_publicExponent = field[4].GetUInt16();

			const char *pubModulusStr = field[5].GetString();
			if (pubModulusStr != NULL)
				m_publicModulus = string(pubModulusStr, 96);
			else
				m_publicModulus.clear();

			const char *privExponentStr = field[6].GetString();
			if (privExponentStr != NULL)
				m_privateExponent = string(field[6].GetString(), 96);
			else
				m_privateExponent.clear();

			m_timeCreated = field[7].GetUInt32();
		}
	}

	if (m_isNewUser)
	{
		sAuth.CreateAccount(m_username, thePassword);
		m_userId = sAuth.getAccountIdForUsername(m_username);
		INFO_LOG(format("Successfully registered user %1%, proceeding to log them in.") % m_username);
		m_publicExponent = 0;
	}
	else if (!AcceptPassword(thePassword))
	{
		WARNING_LOG(format("User %1% supplied an invalid password, disconnecting.") % m_username);
		SetCloseAndDelete(true);
		return;
	}

	// Always generate a fresh, mathematically sound 768-bit RSA keypair with e=17 for this session
	for (;;)
	{
		CryptoPP::AutoSeededRandomPool randPool;
		CryptoPP::InvertibleRSAFunction params;
		params.GenerateRandomWithKeySize(randPool, 768);
		CryptoPP::RSA::PublicKey userPubKey(params);
		CryptoPP::RSA::PrivateKey userPrivKey(params);
		m_publicExponent = uint16(userPubKey.GetPublicExponent().ConvertToLong()); 
		byte tempBuf[96];
		userPubKey.GetModulus().Encode(tempBuf,sizeof(tempBuf));
		m_publicModulus = string((const char*)tempBuf,sizeof(tempBuf));
		m_privateExponent.clear();
		CryptoPP::StringSink privateExponentSink(m_privateExponent);
		userPrivKey.GetPrivateExponent().Encode(privateExponentSink,userPrivKey.GetPrivateExponent().MinEncodedSize());

		if (m_publicExponent == 17 && m_publicModulus.size() == 96 && m_privateExponent.size() == 96)
		{
			break;
		}
	}

	signedDataStruct signedData;
	memset(&signedData,0,sizeof(signedData));
	signedData.unknownByte = 1;
	signedData.userId1 = m_userId;
	strncpy(signedData.userName,m_username.c_str(),sizeof(signedData.userName)-1);
	signedData.unknownShort = 256;
	signedData.expiryTime = getTime() + 60 * 10;
	signedData.publicExponent = swap16(m_publicExponent);
	memcpy(signedData.modulus,m_publicModulus.data(),sizeof(signedData.modulus));

	CryptoPP::Weak::MD5 md5Object;
	md5Object.Update((const byte*)&signedData,sizeof(signedData));
	byte signMePlease[16];
	md5Object.Final(signMePlease);
	ByteBuffer signature = sAuth.SignWith1024Bit(signMePlease,sizeof(signMePlease));

	// Encrypt 96-byte user private exponent using Twofish session cipher with 0 IV
	m_tfEngine.SetEncryptionIV();
	ByteBuffer encryptedPrivateExponent = m_tfEngine.Encrypt((const byte*)m_privateExponent.data(),m_privateExponent.size(),false);

	if (encryptedPrivateExponent.size() != 96)
	{
		ERROR_LOG(format("Encrypted private exponent ended up being something other than 96 bytes (%1% bytes), disconnecting.")
			% encryptedPrivateExponent.size());
		SetCloseAndDelete(true);
		return;
	}

	// Construct AS_AuthReply packet (Opcode 0x09)
	TCPVariableLengthPacket worldPacket;

#pragma pack(push,1)
	typedef struct 
	{
		uint8 opcode;              // 0x00: AS_AuthReply (0x09)
		uint32 status;             // 0x01..0x04: 0 = Success
		uint16 unknown1;           // 0x05..0x06: 0
		uint32 userId;             // 0x07..0x0A: m_userId
		uint16 offsetAuthData;      // 0x0B..0x0C: offset to auth ticket
		uint16 offsetEncryptedData; // 0x0D..0x0E: offset to encrypted private key
		uint16 unknown2;           // 0x0F..0x10: 0
		uint16 unknown3;           // 0x11..0x12: 0
		uint16 offsetWorldData;    // 0x13..0x14: offset to world data (starts at world count)
		uint16 offsetCharData;     // 0x15..0x16: offset to character data (starts at char count)
		uint16 offsetUsername;     // 0x17..0x18: offset to username string
	} AuthReplyHeader;
#pragma pack(pop)

	AuthReplyHeader packetHeader;
	memset(&packetHeader, 0, sizeof(packetHeader));
	packetHeader.opcode = AS_AuthReply;
	packetHeader.status = 0;
	packetHeader.userId = m_userId;

	// Placeholder header (will be rewritten at offset 0 after offsets are computed)
	worldPacket.append((const byte*)&packetHeader, sizeof(packetHeader));

	// 1. Auth Ticket Section (offset 0x0B in header)
	packetHeader.offsetAuthData = (uint16)worldPacket.wpos();
	worldPacket << uint16(signature.size() + sizeof(signedData)); // 306 = 0x132
	worldPacket.append(signature);
	worldPacket.append((const byte*)&signedData, sizeof(signedData));

	// 3. Encrypted Private Key Section (offset 0x0D in header)
	packetHeader.offsetEncryptedData = (uint16)worldPacket.wpos();
	worldPacket << uint16(encryptedPrivateExponent.size()); // 96
	worldPacket.append(encryptedPrivateExponent.contents(), encryptedPrivateExponent.size());

	// 4. Worlds Section (offset 0x13 in header)
	packetHeader.offsetWorldData = (uint16)worldPacket.wpos();

	PreparedStatement stmt2("SELECT `worldId`, `name`, `type`, `status`, `numPlayers` FROM `worlds`");
	scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt2));
	if (result == NULL || result->GetRowCount() < 1)
	{
		result.reset(sDatabase.Query("SELECT `worldId`, `name`, `type`, `status`, `numPlayers` FROM `worlds`"));
	}
	if (result == NULL || result->GetRowCount() < 1)
	{
		ERROR_LOG("No worlds in db, disconnecting.");
		SetCloseAndDelete(true);
		return;
	}

	uint16 numWorlds = (uint16)result->GetRowCount();
	worldPacket << uint16(numWorlds);

#pragma pack(push,1)
	typedef struct  
	{
		uint8 unknown1;          // 0x00: 0
		uint16 nameStrOffset;    // 0x01..0x02: relative offset from &WorldData[i] to (uint16 len + worldName)
		uint8 pad[8];            // 0x03..0x0A: 0
		uint8 status;            // 0x0B: 0x31..0x33
		uint16 worldId;          // 0x0C..0x0D: worldId
	} WorldData;
#pragma pack(pop)

	ByteBuffer worldDatas;
	ByteBuffer worldStrings;

	for (uint i = 0; i < numWorlds; i++)
	{
		Field *field = result->Fetch();
		WorldData currWorld;
		memset(&currWorld, 0, sizeof(currWorld));
		currWorld.unknown1 = 0;
		currWorld.worldId = field[0].GetUInt16();
		string worldNameStr = field[1].GetString();
		uint8 status = field[3].GetUInt8();
		// In client matrix.exe:0x0040C34C, GetWorldStatus() is checked:
		// test eax, eax -> je 0x40c35b (0 = Normal/Open)
		// cmp eax, 6    -> je 0x40c35b (6 = Open)
		// Any other value (e.g. 1 = 'Char In Transit') causes character selection to fail!
		currWorld.status = (status == 6) ? 6 : 0;
		currWorld.nameStrOffset = (numWorlds - i) * sizeof(WorldData) + worldStrings.wpos();

		worldDatas.append((const byte*)&currWorld, sizeof(currWorld));
		worldStrings.writeString(worldNameStr);

		if (!result->NextRow())
			break;
	}

	worldPacket.append(worldDatas);
	worldPacket.append(worldStrings);

	// 5. Characters Section (offset 0x15 in header)
	packetHeader.offsetCharData = (uint16)worldPacket.wpos();

	PreparedStatement stmt("SELECT `charId`, `worldId`, `status`, `handle`, `profession`, `alignment` FROM `characters` WHERE `userId` = ?0 ORDER BY `charId` ASC");
	stmt.SetUInt32(0, m_userId);
	scoped_ptr<QueryResult> charResult(sDatabase.QueryPrepared(&stmt));
	if (charResult == NULL || charResult->GetRowCount() == 0)
	{
		charResult.reset(sDatabase.Query(format("SELECT `charId`, `worldId`, `status`, `handle`, `profession`, `alignment` FROM `characters` WHERE `userId` = %1% ORDER BY `charId` ASC") % m_userId));
	}
	uint16 numCharacters = (charResult == NULL) ? 0 : charResult->GetRowCount();

	// In-game character creation: do not auto-create on login
	

	worldPacket << uint16(numCharacters);

	if (numCharacters > 0)
	{
#pragma pack(push,1)
		typedef struct  
		{
			uint8 unknown1;          // 0x00: 0
			uint16 worldId;          // 0x01..0x02: worldId
			char handle[19];         // 0x03..0x15: character handle
			uint8 nullTerm;          // 0x16: 0
			uint8 status;            // 0x17: Character access status: 1 = Open, 2 = Admins Only (client.dll:0x0040CA29)
			uint8 pad[5];            // 0x18..0x1C: 0
			uint8 faction;           // 0x1D: 1 = Zion, 2 = Machine, 3 = Merovingian (matrix.exe:0x00428F65)
		} CharacterData;
#pragma pack(pop)

		ByteBuffer characterDatas;
		ByteBuffer characterStrings;

		for (uint i = 0; i < numCharacters; i++)
		{
			Field *field = charResult->Fetch();
			CharacterData currCharacter;
			memset(&currCharacter, 0, sizeof(currCharacter));
			currCharacter.unknown1 = 0;
			currCharacter.worldId = field[1].GetUInt16();
			string handleStr = field[3].GetString();
			strncpy(currCharacter.handle, handleStr.c_str(), sizeof(currCharacter.handle) - 1);
			currCharacter.nullTerm = 0;
			// Byte 0x17 was previously confused with discipline/profession.
			// In matrix.exe:0x0040C324: GetCharacterStatus() must be 1 ('Open').
			// If 2 ('Admins Only'), it rejects normal users and fails character selection.
			currCharacter.status = 1;
			uint8 align = field[5].GetUInt8();
			currCharacter.faction = (align > 0) ? align : 1;

			worldPacket.append((const byte*)&currCharacter, sizeof(currCharacter));

			if (!charResult->NextRow())
				break;
		}
	}

	// 6. Username Section (offset 0x17 in header)
	packetHeader.offsetUsername = (uint16)worldPacket.wpos();
	worldPacket << uint16(m_username.length() + 1);
	worldPacket.append((const byte*)m_username.c_str(), m_username.length() + 1);

	// Rewrite finalized header at offset 0
	worldPacket.put(0, (const byte*)&packetHeader, sizeof(packetHeader));

	DEBUG_LOG(format("Sending AS_AuthReply (Opcode 0x09): |%1%|") % Bin2Hex(worldPacket));
	SendPacket(worldPacket);
	}
	catch (const std::exception& e)
	{
		ERROR_LOG(format("Auth request parse/crypto failed (%1%), disconnecting.") % e.what());
		SetCloseAndDelete(true);
	}
	catch (...)
	{
		ERROR_LOG("Auth request processing failed (unknown exception), disconnecting.");
		SetCloseAndDelete(true);
	}
}

void AuthSocket::HandleAuthChallengeResponse( ByteBuffer &packet )
{
	// Retail matrix.exe sends credentials in AS_AuthRequest and expects AS_AuthReply directly.
}

void AuthSocket::HandleCreateCharacterRequest( ByteBuffer &packet )
{
	if (m_userId == 0)
	{
		WARNING_LOG("Auth: Unauthenticated CreateCharacterRequest (m_userId == 0), dropping");
		SetCloseAndDelete(true);
		return;
	}

	try
	{
		if (packet.remaining() < 2)
		{
			WARNING_LOG(format("Auth received malformed CreateCharacter packet (size %1% bytes)") % packet.remaining());
			return;
		}

		const byte* raw = (const byte*)&packet.contents()[packet.rpos()];
		size_t remaining = packet.remaining();

		string handle;
		// If 4+ bytes and has length prefix: [uint16 offset] [uint16 handleLen] [chars...]
		if (remaining >= 4)
		{
			uint16 len16 = *(uint16*)&raw[2];
			if (len16 > 0 && len16 <= remaining - 4 && (raw[4] >= 32 && raw[4] <= 126))
			{
				size_t sLen = len16;
				while (sLen > 0 && raw[4 + sLen - 1] == '\0') sLen--;
				handle = string((const char*)&raw[4], sLen);
			}
		}

		// If still empty, try scanning printable characters
		if (handle.empty())
		{
			for (size_t i = 0; i < remaining; ++i)
			{
				if (isalnum((unsigned char)raw[i]) || raw[i] == '_' || raw[i] == '-')
				{
					size_t start = i;
					while (i < remaining && raw[i] != '\0' && isprint((unsigned char)raw[i]))
						i++;
					handle = string((const char*)&raw[start], i - start);
					break;
				}
			}
		}

		// Enforce strict handle length and character validation: 3 <= len <= 24, ^[a-zA-Z0-9_-]+$
		bool validFormat = (handle.size() >= 3 && handle.size() <= 24);
		if (validFormat)
		{
			for (char c : handle)
			{
				if (!isalnum((unsigned char)c) && c != '_' && c != '-')
				{
					validFormat = false;
					break;
				}
			}
		}

		if (!validFormat)
		{
			WARNING_LOG(format("Auth: Invalid handle '%1%' (len %2%), rejecting prior to SQL query") % handle % handle.size());
			string safeReplyHandle = handle.substr(0, std::min<size_t>(handle.size(), 24));
			vector<char> hBuf(safeReplyHandle.begin(), safeReplyHandle.end());
			hBuf.push_back('\0');
			uint16 hLen = (uint16)hBuf.size();

			TCPVariableLengthPacket replyPacket;
			replyPacket << uint8(AS_CreateCharacterReply); // 0x0B
			replyPacket << uint16(0x000F);                 // String offset table (0x0F)
			replyPacket << uint32(1);                      // status 1 = Failed
			replyPacket << uint64(0);
			replyPacket << uint16(hLen);
			replyPacket.append((const byte*)hBuf.data(), hLen);
			SendPacket(replyPacket);
			return;
		}

		DEBUG_LOG(format("HandleCreateCharacterRequest: Account='%1%', Handle='%2%'") % m_username % handle);

		string worldName = "Reality";
		string firstName = handle;
		string lastName = "Operative";
		if (handle.size() > 24) handle = handle.substr(0, 24);
		if (firstName.size() > 24) firstName = firstName.substr(0, 24);
		if (lastName.size() > 24) lastName = lastName.substr(0, 24);

		uint64 newCharId = 0;
		uint64 existingCharId = sAuth.getCharIdForHandle(handle);

		bool success = false;
		if (existingCharId != 0)
		{
			// Check if this character already belongs to the current user
			PreparedStatement checkStmt("SELECT `charId` FROM `characters` WHERE `handle` = ?0 AND `userId` = ?1");
			checkStmt.SetString(0, handle);
			checkStmt.SetUInt32(1, m_userId);
			scoped_ptr<QueryResult> checkRes(sDatabase.QueryPrepared(&checkStmt));
			if (checkRes)
			{
				newCharId = existingCharId;
				success = true;
			}
			else
			{
				// Character name taken by another account
				success = false;
			}
		}
		else
		{
			// Create character record
			PreparedStatement stmt("INSERT INTO `characters` (`userId`, `worldId`, `status`, `handle`, `firstName`, `lastName`, `background`, `x`, `y`, `z`, `rot`, `healthC`, `healthM`, `innerStrC`, `innerStrM`, `level`, `profession`, `alignment`, `pvpflag`, `exp`, `cash`, `district`, `adminFlags`) "
				"VALUES (?0, 1, 0, ?1, ?2, ?3, '', 16802.3, 665.0, 3237.01, 0.0245437, 250, 250, 100, 100, 1, 1, 0, 0, 0, 250, 1, 0)");
			stmt.SetUInt32(0, m_userId);
			stmt.SetString(1, handle);
			stmt.SetString(2, firstName);
			stmt.SetString(3, lastName);
			success = sDatabase.ExecutePrepared(&stmt);

			if (success)
			{
				newCharId = sAuth.getCharIdForHandle(handle);
				if (newCharId != 0)
				{
					PreparedStatement rsiStmt("INSERT IGNORE INTO `rsivalues` (`charId`, `sex`, `body`, `hat`, `face`, `shirt`, `coat`, `pants`, `shoes`, `gloves`, `glasses`, `hair`, `facialdetail`, `shirtcolor`, `pantscolor`, `coatcolor`, `shoecolor`, `glassescolor`, `haircolor`, `skintone`, `tattoo`, `facialdetailcolor`, `leggings`) "
						"VALUES (?0, 0, 2, 0, 0, 2, 10, 1, 6, 6, 4, 0, 0, 41, 16, 0, 0, 15, 0, 0, 0, 0, 0)");
					rsiStmt.SetUInt64(0, newCharId);
					sDatabase.ExecutePrepared(&rsiStmt);
				}
			}
		}

		// AS_CreateCharacterReply (0x0B) format expected by matrix.exe at 0x43f4c0:
		// [uint8 opcode 0x0B] [uint16 0x000F] [uint32 status (0=success)] [uint64 charId] [uint16 handleLen] [handle chars...]
		vector<char> hBuf(handle.begin(), handle.end());
		hBuf.push_back('\0');
		uint16 hLen = (uint16)hBuf.size();

		TCPVariableLengthPacket replyPacket;
		replyPacket << uint8(AS_CreateCharacterReply); // 0x0B
		replyPacket << uint16(0x000F);                 // String offset table (0x0F)
		if (success && newCharId != 0)
		{
			replyPacket << uint32(0);                  // status 0 = Success
			replyPacket << uint64(newCharId);          // 64-bit charId
			replyPacket << uint16(hLen);
			replyPacket.append((const byte*)hBuf.data(), hLen);
			INFO_LOG(format("Auth: Character '%1%' (charId %2%) successfully created for user %3%!") % handle % newCharId % m_username);
		}
		else
		{
			replyPacket << uint32(1);                  // status 1 = Failed (Name in use / Error)
			replyPacket << uint64(0);
			replyPacket << uint16(hLen);
			replyPacket.append((const byte*)hBuf.data(), hLen);
			WARNING_LOG(format("Auth: Failed to create character '%1%' for user %2%!") % handle % m_username);
		}

		SendPacket(replyPacket);
	}
	catch (const std::exception &ex)
	{
		ERROR_LOG(format("Auth: Exception in HandleCreateCharacterRequest: %1%") % ex.what());
		TCPVariableLengthPacket replyPacket;
		replyPacket << uint8(AS_CreateCharacterReply);
		replyPacket << uint16(0x000F);
		replyPacket << uint32(1);
		replyPacket << uint64(0);
		SendPacket(replyPacket);
	}
}

void AuthSocket::HandleDeleteCharacterRequest( ByteBuffer &packet )
{
	if (m_userId == 0)
	{
		WARNING_LOG("Auth: Unauthenticated DeleteCharacterRequest (m_userId == 0), dropping");
		SetCloseAndDelete(true);
		return;
	}

	if (packet.remaining() < sizeof(uint64))
	{
		SetCloseAndDelete(true);
		return;
	}

	uint64 delCharId;
	packet >> delCharId;

	DEBUG_LOG(format("AuthSocket::HandleDeleteCharacterRequest: User %1% deleting charId %2%") % m_username % delCharId);

	PreparedStatement stmt("DELETE FROM `characters` WHERE `charId` = ?0 AND `userId` = ?1");
	stmt.SetUInt64(0, delCharId);
	stmt.SetUInt32(1, m_userId);
	bool success = sDatabase.ExecutePrepared(&stmt);

	if (success)
	{
		PreparedStatement stmtRsi("DELETE FROM `rsivalues` WHERE `charId` = ?0");
		stmtRsi.SetUInt64(0, delCharId);
		sDatabase.ExecutePrepared(&stmtRsi);
	}

	TCPVariableLengthPacket replyPacket;
	replyPacket << uint8(AS_DeleteCharacterReply);
	replyPacket << uint8(success ? 0x00 : 0x01);
	replyPacket << uint64(delCharId);
	SendPacket(replyPacket);
}


// ===========================================================================
// 7.6005 client auth (retail protocol, from the original mxoemu implementation)
//   C->S AS_AuthRequest (0x08): RSA-OAEP blob
//        [u8 0][u32 method=4][u16][16 twofish key][u32 server time][u16 len][username\0]
//   S->C AS_AuthChallenge (0x09): Twofish(key, IV=0) of 16 random bytes
//   C->S AS_AuthChallengeResponse (0x0A): [u16][u16 len] Twofish(key, IV=0)
//        [u8][16 md5(challenge)][u16][u16][u16][u16 len][password\0][u16 len][soePass][u16 len][padding]
//   S->C AS_AuthReply (0x0B): header, characters, worlds, signed ticket ("36 01"),
//        user private exponent (Twofish, IV = challenge as sent), username
// ===========================================================================
// The live DB pool sometimes returns an empty result for the first query on a
// connection (seen as "user not found" / "No worlds in db" for valid data), so the
// 7.6005 path retries each read once.
static QueryResult* QueryWithRetry(const string& sql)
{
	QueryResult* r = sDatabase.Query(sql);
	if (r == NULL || r->GetRowCount() == 0)
	{
		delete r;
		r = sDatabase.Query(sql);
	}
	return r;
}

void AuthSocket::HandleAuthRequest76005( ByteBuffer &packet )
{
	try
	{
#pragma pack(push,1)
		typedef struct
		{
			uint32 rsaType;
			uint32 unknownz1;
			char unknownz2[31];
			uint16 blobLen;
		} requestHdr;
#pragma pack(pop)

		requestHdr requestHeader;
		memset(&requestHeader,0,sizeof(requestHeader));
		if (packet.remaining() < sizeof(requestHeader))
		{
			SetCloseAndDelete(true);
			return;
		}
		packet.read((uint8 *)&requestHeader,sizeof(requestHeader));
		if (requestHeader.blobLen == 0 || packet.remaining() < requestHeader.blobLen)
		{
			SetCloseAndDelete(true);
			return;
		}

		vector<byte> encryptedBlob(requestHeader.blobLen);
		packet.read(&encryptedBlob[0],encryptedBlob.size());

		string decryptedBlob;
		try
		{
			decryptedBlob = sAuth.Decrypt(string((const char*)&encryptedBlob[0],encryptedBlob.size()));
		}
		catch (CryptoPP::Exception&)
		{
			ERROR_LOG("7.6005 AuthRequest: invalid RSA ciphertext (client pubkey.dat does not match this server), disconnecting.");
			SetCloseAndDelete(true);
			return;
		}
		if (decryptedBlob.size() < 1 + 4 + 2 + 16 + 4 + 2)
		{
			ERROR_LOG("7.6005 AuthRequest: blob too short, disconnecting.");
			SetCloseAndDelete(true);
			return;
		}

		ByteBuffer blob(decryptedBlob.substr(1));
		uint32 rsaMethod; blob >> rsaMethod;
		if (rsaMethod != 4)
			WARNING_LOG(format("7.6005 AuthRequest: rsaMethod %1% (expected 4)") % rsaMethod);
		uint16 someShort; blob >> someShort;

		byte twofishKey[16];
		blob.read(twofishKey,sizeof(twofishKey));
		m_tfEngine.Initialize(twofishKey,sizeof(twofishKey));

		uint32 theTime; blob >> theTime;
		uint16 usernameLen; blob >> usernameLen;
		if (usernameLen < 2 || blob.remaining() < usernameLen)
		{
			ERROR_LOG("7.6005 AuthRequest: bad username field, disconnecting.");
			SetCloseAndDelete(true);
			return;
		}
		vector<char> usernameVect(usernameLen);
		blob.read((uint8 *)&usernameVect[0],usernameVect.size());
		m_username = string(&usernameVect[0], strnlen(&usernameVect[0], usernameVect.size()));
		DEBUG_LOG(format("7.6005 AuthRequest: user |%1%|") % m_username);

		m_isNewUser = false;
		{
			scoped_ptr<QueryResult> result(QueryWithRetry((format("SELECT `userId`, `username`, `passwordSalt`, `passwordHash`, `timeCreated` FROM `users` WHERE LOWER(`username`) = LOWER('%1%') LIMIT 1") % sDatabase.EscapeString(m_username)).str()));
			if (result == NULL || result->GetRowCount() == 0)
			{
				// Same behaviour as the 2005 path: unknown users are registered on first login
				// (after the password arrives in the challenge response).
				m_isNewUser = true;
				m_userId = 0;
				m_timeCreated = getTime();
			}
			else
			{
				Field *field = result->Fetch();
				m_userId = field[0].GetUInt32();
				m_username = field[1].GetString();
				m_passwordSalt = field[2].GetString();
				m_passwordHash = field[3].GetString();
				m_timeCreated = field[4].GetUInt32();
			}
		}

		// Challenge: 16 random bytes, sent Twofish-encrypted (IV 0). The client decrypts
		// and returns md5(plain); the bytes as sent are the IV for the private-key blob.
		byte plainChallenge[16];
		CryptoPP::AutoSeededRandomPool randPool;
		randPool.GenerateBlock(plainChallenge,sizeof(plainChallenge));

		m_tfEngine.SetEncryptionIV();
		ByteBuffer ourChallenge = m_tfEngine.Encrypt(plainChallenge,sizeof(plainChallenge),false);
		if (ourChallenge.size() != sizeof(challenge))
		{
			ERROR_LOG("7.6005 AuthRequest: challenge encryption failed, disconnecting.");
			SetCloseAndDelete(true);
			return;
		}
		memcpy(challenge,ourChallenge.contents(),sizeof(challenge));

		CryptoPP::Weak::MD5 md5;
		md5.Update(plainChallenge,sizeof(plainChallenge));
		md5.Final(finalChallenge);

		TCPVariableLengthPacket reply;
		reply << byte(AS76_AuthChallenge);
		reply.append(ourChallenge.contents(),ourChallenge.size());
		SendPacket(reply);
		DEBUG_LOG(format("Sending AS_AuthChallenge (7.6005): |%1%|") % Bin2Hex(reply));
	}
	catch (const std::exception& e)
	{
		ERROR_LOG(format("7.6005 AuthRequest failed (%1%), disconnecting.") % e.what());
		SetCloseAndDelete(true);
	}
}

void AuthSocket::HandleAuthChallengeResponse76005( ByteBuffer &packet )
{
	try
	{
		uint16 someShort; packet >> someShort;
		uint16 cipherTextLen; packet >> cipherTextLen;
		if (cipherTextLen == 0 || (cipherTextLen % 16) != 0 || packet.remaining() < cipherTextLen)
		{
			ERROR_LOG(format("7.6005 ChallengeResponse: bad ciphertext length %1%, disconnecting.") % cipherTextLen);
			SetCloseAndDelete(true);
			return;
		}
		vector<byte> cipherText(cipherTextLen);
		packet.read(&cipherText[0],cipherText.size());

		m_tfEngine.SetDecryptionIV();
		ByteBuffer plain = m_tfEngine.Decrypt(&cipherText[0],cipherText.size(),false);

		uint8 someByte; plain >> someByte;
		byte processedChallenge[16];
		plain.read(processedChallenge,sizeof(processedChallenge));
		if (memcmp(processedChallenge,finalChallenge,sizeof(processedChallenge)) != 0)
		{
			WARNING_LOG(format("7.6005 ChallengeResponse: challenge mismatch for %1%, disconnecting.") % m_username);
			SetCloseAndDelete(true);
			return;
		}

		uint16 unknown1, unknown2, unknown3;
		plain >> unknown1 >> unknown2 >> unknown3;
		uint16 passwordLen; plain >> passwordLen;
		if (passwordLen == 0 || plain.remaining() < passwordLen)
		{
			ERROR_LOG("7.6005 ChallengeResponse: bad password field, disconnecting.");
			SetCloseAndDelete(true);
			return;
		}
		vector<char> password(passwordLen);
		plain.read((byte*)&password[0],password.size());
		string thePassword(&password[0], strnlen(&password[0], password.size()));

		if (m_isNewUser)
		{
			sAuth.CreateAccount(m_username, thePassword);
			m_userId = sAuth.getAccountIdForUsername(m_username);
			INFO_LOG(format("7.6005: registered new user %1% (userId=%2%).") % m_username % m_userId);
		}
		else if (!AcceptPassword(thePassword))
		{
			WARNING_LOG(format("7.6005: user %1% supplied an invalid password, disconnecting.") % m_username);
			SetCloseAndDelete(true);
			return;
		}

		SendAuthReply76005();
	}
	catch (const std::exception& e)
	{
		ERROR_LOG(format("7.6005 ChallengeResponse failed (%1%), disconnecting.") % e.what());
		SetCloseAndDelete(true);
	}
}

void AuthSocket::SendAuthReply76005()
{
	// Fresh 768-bit user keypair (e=17) per session, same as the 2005 path.
	for (;;)
	{
		CryptoPP::AutoSeededRandomPool randPool;
		CryptoPP::InvertibleRSAFunction params;
		params.GenerateRandomWithKeySize(randPool, 768);
		CryptoPP::RSA::PublicKey userPubKey(params);
		CryptoPP::RSA::PrivateKey userPrivKey(params);
		m_publicExponent = uint16(userPubKey.GetPublicExponent().ConvertToLong());
		byte tempBuf[96];
		userPubKey.GetModulus().Encode(tempBuf,sizeof(tempBuf));
		m_publicModulus = string((const char*)tempBuf,sizeof(tempBuf));
		m_privateExponent.clear();
		CryptoPP::StringSink privateExponentSink(m_privateExponent);
		userPrivKey.GetPrivateExponent().Encode(privateExponentSink,userPrivKey.GetPrivateExponent().MinEncodedSize());
		if (m_publicExponent == 17 && m_publicModulus.size() == 96 && m_privateExponent.size() == 96)
			break;
	}

	signedDataStruct76005 signedData;
	memset(&signedData,0,sizeof(signedData));
	signedData.base.unknownByte = 1;
	signedData.base.userId1 = m_userId;
	strncpy(signedData.base.userName,m_username.c_str(),sizeof(signedData.base.userName)-1);
	signedData.base.unknownShort = 256;
	signedData.base.expiryTime = getTime() + 60 * 10;
	signedData.base.publicExponent = swap16(m_publicExponent);
	memcpy(signedData.base.modulus,m_publicModulus.data(),sizeof(signedData.base.modulus));
	signedData.timeCreated = m_timeCreated;

	CryptoPP::Weak::MD5 md5Object;
	md5Object.Update((const byte*)&signedData,sizeof(signedData));
	byte signMePlease[16];
	md5Object.Final(signMePlease);
	ByteBuffer signature = sAuth.SignWith1024Bit(signMePlease,sizeof(signMePlease));

	// Private exponent: Twofish with the session key, IV = the challenge bytes as sent.
	m_tfEngine.SetEncryptionIV(challenge,sizeof(challenge));
	ByteBuffer encryptedPrivateExponent = m_tfEngine.Encrypt((const byte*)m_privateExponent.data(),m_privateExponent.size(),false);
	if (encryptedPrivateExponent.size() != 96)
	{
		ERROR_LOG(format("7.6005 AuthReply: encrypted private exponent is %1% bytes, disconnecting.") % encryptedPrivateExponent.size());
		SetCloseAndDelete(true);
		return;
	}

#pragma pack(push,1)
	typedef struct
	{
		uint8 opcode;               // AS_AuthReply (0x0B)
		byte unknown1[10];          // zero
		uint16 offsetAuthData;      // where the signed ticket starts
		uint16 offsetEncryptedData; // where the encrypted private exponent starts
		uint32 unknown2;            // always 0x1F
		uint16 offsetCharData;      // = sizeof(header)
		uint32 unknown3;            // always 0xD16E
		uint32 offsetServerData;    // where the world list starts
		uint32 offsetUsername;      // where the trailing username starts
	} AuthReplyHeader76005;

	typedef struct
	{
		uint8 unknown1;             // 0
		uint16 handleStrOffset;     // from this entry to its handle string
		uint64 charId;
		uint8 status;
		uint16 worldId;
	} CharacterData76005;

	typedef struct
	{
		uint8 unknown1;             // 0
		uint16 worldId;
		char worldName[20];
		uint8 status;
		uint8 type;
		uint32 clientVersion;       // echo of the client's build
		uint16 unknown4;            // 1
		uint8 load;                 // 0x31..0x33
	} WorldData76005;
#pragma pack(pop)

	AuthReplyHeader76005 header;
	memset(&header,0,sizeof(header));
	header.opcode = AS76_AuthReply;
	header.unknown2 = 0x1F;
	header.unknown3 = 0x0000D16E;
	header.offsetCharData = sizeof(header);

	TCPVariableLengthPacket pkt;
	pkt.append((const byte*)&header,sizeof(header));

	// Characters
	{
		scoped_ptr<QueryResult> result(QueryWithRetry((format("SELECT `charId`, `worldId`, `status`, `handle` FROM `characters` WHERE `userId` = %1% ORDER BY `charId` ASC") % m_userId).str()));
		uint16 numCharacters = (result == NULL) ? 0 : uint16(result->GetRowCount());
		pkt << uint16(numCharacters);
		if (numCharacters > 0)
		{
			ByteBuffer datas, strings;
			for (uint16 i = 0; i < numCharacters; i++)
			{
				Field *field = result->Fetch();
				CharacterData76005 c;
				memset(&c,0,sizeof(c));
				c.charId = field[0].GetUInt64();
				c.worldId = field[1].GetUInt16();
				c.status = 0; // 0 = ok (the 2005 path uses its own status values; do not leak them here)
				c.handleStrOffset = uint16((numCharacters - i) * sizeof(CharacterData76005) + strings.wpos());
				datas.append((const byte*)&c,sizeof(c));
				strings.writeString(string(field[3].GetString()));
				if (!result->NextRow())
					break;
			}
			pkt.append(datas);
			pkt.append(strings);
		}
	}

	// Worlds
	header.offsetServerData = uint32(pkt.wpos());
	{
		scoped_ptr<QueryResult> result(QueryWithRetry("SELECT `worldId`, `name`, `type`, `status`, `numPlayers` FROM `worlds`"));
		if (result == NULL || result->GetRowCount() < 1)
		{
			ERROR_LOG("7.6005 AuthReply: no worlds in db, disconnecting.");
			SetCloseAndDelete(true);
			return;
		}
		pkt << uint16(result->GetRowCount());
		do
		{
			Field *field = result->Fetch();
			WorldData76005 w;
			memset(&w,0,sizeof(w));
			w.worldId = field[0].GetUInt16();
			string name = field[1].GetString();
			strncpy(w.worldName,name.c_str(),sizeof(w.worldName)-1);
			w.type = field[2].GetUInt8();
			w.status = field[3].GetUInt8();
			w.clientVersion = matrixVersion;
			w.unknown4 = 1;
			uint32 numPlayers = field[4].GetUInt32();
			w.load = numPlayers < 50 ? 0x31 : (numPlayers < 100 ? 0x32 : 0x33);
			pkt.append((const byte*)&w,sizeof(w));
		}
		while (result->NextRow());
	}

	// Signed ticket
	header.offsetAuthData = uint16(pkt.wpos());
	pkt << uint16(signature.size() + sizeof(signedData)); // 0x0136
	pkt.append(signature);
	pkt.append((const byte*)&signedData,sizeof(signedData));

	// Encrypted private exponent
	header.offsetEncryptedData = uint16(pkt.wpos());
	pkt << uint16(encryptedPrivateExponent.size());
	pkt.append(encryptedPrivateExponent.contents(),encryptedPrivateExponent.size());

	// Username
	header.offsetUsername = uint32(pkt.wpos());
	pkt.writeString(m_username);

	pkt.put(0,(const byte*)&header,sizeof(header));

	DEBUG_LOG(format("Sending AS_AuthReply (7.6005, 0x0B): |%1%|") % Bin2Hex(pkt));
	SendPacket(pkt);
	INFO_LOG(format("7.6005: user %1% (userId=%2%) authenticated.") % m_username % m_userId);
}
