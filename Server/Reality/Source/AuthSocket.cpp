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

AuthSocket::AuthSocket( ISocketHandler& h ) : TCPVarLenSocket(h), m_isNewUser(false)
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

void AuthSocket::ProcessData( const byte *buf,size_t len )
{
	ByteBuffer packetContents(buf,len);

	DEBUG_LOG(format("Auth Receieved |%1%|") % Bin2Hex(packetContents));

	byte packetOpcode;
	packetContents >> packetOpcode;
	AuthOpcode opcode = AuthOpcode(packetOpcode);

	switch (opcode)
	{
	default:
		{
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
			HandleCreateCharacterRequest(packetContents);
			break;
		}
	case AS_DeleteCharacterRequest:
		{
			HandleDeleteCharacterRequest(packetContents);
			break;
		}
	}
}

bool AuthSocket::VerifyPassword( const string& plaintextPass, const string& passwordSalt, const string& passwordHash )
{
	if (sAuth.HashPassword(passwordSalt,plaintextPass) == passwordHash)
		return true;

	return false;
}

void AuthSocket::HandleGetPublicKeyRequest( ByteBuffer &packet )
{
	packet >> matrixVersion;
	string clientVersionStr = ClientVersionString(matrixVersion);
	if (clientVersionStr != "7.5668" )
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

	uint32 rsaBlobMethod;
	rsaBlobBuffer >> rsaBlobMethod;
	if (rsaBlobMethod != 4)
	{
		WARNING_LOG("rsaMethod in rsaBlob not 4!");
	}

	byte twofishKey[16];
	rsaBlobBuffer.read(twofishKey,sizeof(twofishKey));
	DEBUG_LOG(format("Auth TF key: |%s|") % Bin2Hex(twofishKey,sizeof(twofishKey)));

	// Instantiate ciphers with client's twofish session key
	m_tfEngine.Initialize(twofishKey,sizeof(twofishKey));

	uint16 usernameLen;
	rsaBlobBuffer >> usernameLen;
	vector<char> usernameVect(usernameLen);
	rsaBlobBuffer.read((uint8 *)&usernameVect[0],usernameVect.size());
	string theUsername(&usernameVect[0],usernameVect.size()-1);
	m_username = theUsername;

	// In retail matrix.exe, the password is provided directly in the RSA blob:
	//   [u32 method][16-byte twofish key][u16 usernameLen][username\0][u16 passLen][pass\0]
	uint16 passwordLen = 0;
	string thePassword;
	if (rsaBlobBuffer.remaining() >= sizeof(uint16))
	{
		rsaBlobBuffer >> passwordLen;
		if (passwordLen > 0 && rsaBlobBuffer.remaining() >= passwordLen)
		{
			vector<char> passVect(passwordLen);
			rsaBlobBuffer.read((uint8*)&passVect[0], passVect.size());
			thePassword = string(&passVect[0], passVect.size() - 1); // strip null terminator
		}
	}
	DEBUG_LOG(format("Auth parsed - User: |%1%|, Pass: |%2%|") % m_username % thePassword);

	// Query user in database
	{
		PreparedStatement stmt("SELECT `userId`, `username`, `passwordSalt`, `passwordHash`, `publicExponent`, `publicModulus`, `privateExponent`, `timeCreated` FROM `users` WHERE LOWER(`username`) = LOWER(?0) LIMIT 1");
		stmt.SetString(0, m_username);
		scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
		if (result == NULL)
		{
			ERROR_LOG("Database query failed during authentication. Disconnecting.");
			SetCloseAndDelete(true);
			return;
		}
		if (result->GetRowCount() == 0)
		{
			m_isNewUser = true;
			if (m_username.length() > 9 && strncasecmp(m_username.c_str(), "register ", 9) == 0)
			{
				m_username = m_username.substr(9);
			}
			INFO_LOG(format("New registration request for user %1%") % m_username);
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
				m_publicModulus = string(pubModulusStr,96);
			else
				m_publicModulus.clear();

			const char *privExponentStr = field[6].GetString();
			if (privExponentStr != NULL)
				m_privateExponent = string(field[6].GetString(),96);
			else
				m_privateExponent.clear();

			m_timeCreated = field[7].GetUInt32();
		}
	}

	if (m_isNewUser)
	{
		sAuth.CreateAccount(m_username, thePassword);
		m_userId = sAuth.getAccountIdForUsername(m_username);
		INFO_LOG(format("Successfully registered new user %1%, proceeding to log them in.") % m_username);
		m_publicExponent = 0;
	}
	else if (!VerifyPassword(thePassword, m_passwordSalt, m_passwordHash))
	{
		WARNING_LOG(format("User %1% supplied an invalid password, disconnecting.") % m_username);
		SetCloseAndDelete(true);
		return;
	}

	if (m_publicExponent != 17 || m_publicModulus.size() != 96 || m_privateExponent.size() != 96)
	{
		INFO_LOG(format("Invalid RSA keys for user %1%, regenerating.") % m_username);
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

		PreparedStatement stmt("UPDATE `users` SET `publicExponent` = ?0, `publicModulus` = ?1, `privateExponent` = ?2 WHERE `userId` = ?3");
		stmt.SetUInt32(0, m_publicExponent);
		stmt.SetString(1, Bin2Hex(m_publicModulus,BIN2HEX_ZEROES));
		stmt.SetString(2, Bin2Hex(m_privateExponent,BIN2HEX_ZEROES));
		stmt.SetUInt32(3, m_userId);
		sDatabase.ExecutePrepared(&stmt);
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
	signedData.timeCreated = m_timeCreated;

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
		uint16 offsetCharData;     // 0x13..0x14: offset to character data
		uint16 offsetWorldData;    // 0x15..0x16: offset to world data
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

	// 1. Characters Section (offset 0x13 in header)
	packetHeader.offsetCharData = worldPacket.wpos();

	PreparedStatement stmt("SELECT `charId`, `worldId`, `status`, `handle` FROM `characters` WHERE `userId` = ?0");
	stmt.SetUInt32(0, m_userId);
	scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
	uint16 numCharacters = (result == NULL) ? 0 : result->GetRowCount();

	// In-game character creation: do not auto-create on login
	

	worldPacket << uint16(numCharacters);

	if (numCharacters > 0)
	{
#pragma pack(push,1)
		typedef struct  
		{
			uint8 unknown1;         // 0
			uint16 handleStrOffset; // offset to character name
			uint64 charId;          // 64-bit character GUID
			uint8 status;           // status byte
			uint16 worldId;         // worldId
		} CharacterData;
#pragma pack(pop)

		ByteBuffer characterDatas;
		ByteBuffer characterStrings;

		for (uint i=0; i<numCharacters; i++)
		{
			Field *field = result->Fetch();
			CharacterData currCharacter;
			currCharacter.unknown1 = 0;
			currCharacter.charId = field[0].GetUInt64();
			currCharacter.worldId = field[1].GetUInt16();
			currCharacter.status = field[2].GetUInt8();
			currCharacter.handleStrOffset = (numCharacters - i) * sizeof(CharacterData) + characterStrings.wpos();

			characterDatas.append((const byte*)&currCharacter, sizeof(currCharacter));

			string characterString = field[3].GetString();
			characterStrings.writeString(characterString);

			if (!result->NextRow())
				break;
		}

		worldPacket.append(characterDatas);
		worldPacket.append(characterStrings);
	}

	// 2. Worlds Section (offset 0x15 in header)
	packetHeader.offsetWorldData = worldPacket.wpos();

	PreparedStatement stmt2("SELECT `worldId`, `name`, `type`, `status`, `numPlayers` FROM `worlds`");
	result.reset(sDatabase.QueryPrepared(&stmt2));
	if (result == NULL || result->GetRowCount() < 1)
	{
		ERROR_LOG("No worlds in db, disconnecting.");
		SetCloseAndDelete(true);
		return;
	}

	uint16 numWorlds = result->GetRowCount();
	worldPacket << uint16(numWorlds);

#pragma pack(push,1)
	typedef struct  
	{
		uint8 unknown1;          // 0
		uint16 worldId;          // world ID
		char worldName[20];      // 20-byte world name string
		uint8 status;            // status
		uint32 clientVersion;    // matrixVersion
		uint8 serverLanguage;    // 0
		uint8 load;              // 0x31..0x33
	} WorldData;
#pragma pack(pop)

	do 
	{
		Field *field = result->Fetch();
		WorldData currWorld;
		memset(&currWorld, 0, sizeof(currWorld));
		currWorld.unknown1 = 0;
		currWorld.worldId = field[0].GetUInt16();
		string worldNameStr = field[1].GetString();
		strncpy(currWorld.worldName, worldNameStr.c_str(), sizeof(currWorld.worldName)-1);
		currWorld.status = field[3].GetUInt8();
		currWorld.clientVersion = matrixVersion;
		currWorld.serverLanguage = 0;

		uint32 numPlayers = field[4].GetUInt32();
		if (numPlayers < 50)
			currWorld.load = 0x31;
		else if (numPlayers < 100)
			currWorld.load = 0x32;
		else 
			currWorld.load = 0x33;

		worldPacket.append((const byte*)&currWorld, sizeof(currWorld));
	} while(result->NextRow());

	// 3. Auth Ticket Section (offset 0x0B in header)
	packetHeader.offsetAuthData = worldPacket.wpos();
	worldPacket << uint16(signature.size() + sizeof(signedData)); // 306 = 0x132
	worldPacket.append(signature);
	worldPacket.append((const byte*)&signedData, sizeof(signedData));

	// 4. Encrypted Private Key Section (offset 0x0D in header)
	packetHeader.offsetEncryptedData = worldPacket.wpos();
	worldPacket << uint16(encryptedPrivateExponent.size()); // 96
	worldPacket.append(encryptedPrivateExponent.contents(), encryptedPrivateExponent.size());

	// 5. Username Section (offset 0x17 in header)
	packetHeader.offsetUsername = worldPacket.wpos();
	worldPacket.writeString(m_username);

	// Rewrite header at offset 0
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
	try
	{
		if (packet.remaining() < 2)
		{
			WARNING_LOG(format("Auth received malformed CreateCharacter packet (size %1% bytes)") % packet.remaining());
			return;
		}

		const byte* raw = &packet.contents()[packet.rpos()];
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

		if (handle.empty())
			handle = m_username;

		DEBUG_LOG(format("HandleCreateCharacterRequest: Account='%1%', Handle='%2%'") % m_username % handle);

		string worldName = "Reality";
		string firstName = handle;
		string lastName = "Operative";

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
				"VALUES (?0, 1, 0, ?1, ?2, ?3, '', 16802.3, 495.0, 3237.01, 0.0245437, 500, 500, 200, 200, 50, 2, 0, 0, 1000000000, 10000, 1, 0)");
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
	if (packet.remaining() < sizeof(uint64))
		return;

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
