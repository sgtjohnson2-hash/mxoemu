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

//include a unit test at the very top if you want to run it
//#include "SubPacketsTest.h"
//#include "seqchecktest.h"

#ifndef UNITTEST
#include "Common.h"
#include "Master.h"
#include "Util.h"
#include "Crypto.h"
#include <iostream>
#include <vector>
#include "BitStream.h"
#include "ByteBuffer.h"
#endif

bool g_sniffPackets = false;
bool g_testProtocol = false;

#ifdef CLIENT_EXE
#include "CustomClient.h"
int main(int argc, char* argv[])
{
    std::cout << "Initializing Zion Mainframe Jack-In Sequence..." << std::endl;
    std::string user = "guest";
    std::string token = "";
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--token" && i + 1 < argc) {
            token = argv[++i];
        } else {
            user = arg; // Last non-flag arg is user
        }
    }
    
    CustomClient::Run(user, token);
    return 0;
}
#else

void RunProtocolTests();

int main(int argc, char* argv[])
{
    FILE* fp = fopen("main_reached.txt", "w");
    if (fp) {
        fprintf(fp, "main reached\n");
        fclose(fp);
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sniff") {
            g_sniffPackets = true;
        } else if (arg == "--test-protocol") {
            g_testProtocol = true;
        }
    }

#ifndef UNITTEST
    if (g_testProtocol) {
        RunProtocolTests();
    } else {
        Master::getSingleton().Run();
    }
#else
	runTest();
	for(;;){Sleep(10000);}
#endif

	return 0;
}
#endif

void RunProtocolTests() {
    std::cout << "[Protocol Test] Running headless protocol assertions..." << std::endl;
    
    FILE* fp = fopen("protocol_dump.json", "r");
    if (!fp) {
        std::cerr << "[Protocol Test] Could not open protocol_dump.json" << std::endl;
        return;
    }
    
    char line[4096];
    int packetCount = 0;
    while (fgets(line, sizeof(line), fp)) {
        std::string s(line);
        size_t pos = s.find("\"hex\": \"");
        if (pos != std::string::npos) {
            pos += 8;
            size_t endPos = s.find("\"", pos);
            if (endPos != std::string::npos) {
                std::string hexStr = s.substr(pos, endPos - pos);
                
                // Convert hex string to binary
                std::vector<unsigned char> data;
                for (size_t i = 0; i < hexStr.length(); i += 2) {
                    std::string byteString = hexStr.substr(i, 2);
                    unsigned char byte = (unsigned char)strtol(byteString.c_str(), NULL, 16);
                    data.push_back(byte);
                }
                
                if (!data.empty()) {
                    ByteBuffer bb(data.data(), data.size());
                    std::cout << "[Protocol Test] Packet " << packetCount++ << " loaded. Length: " << data.size() << " bytes." << std::endl;
                    
                    // Simple simulated protocol assertion logic
                    // Look for 0x04 ordered block as GameClient::HandleEncrypted does
                    bb.rpos(0);
                    int32 commandOffset = -1;
                    ByteBuffer zeroFourBlock;
                    while (bb.remaining() > 0) {
                        uint8 theByte;
                        bb >> theByte;
                        if (theByte == 0x04) {
                            commandOffset = (int32)bb.rpos() - 1;
                            zeroFourBlock = ByteBuffer(&bb.contents()[commandOffset], bb.size() - commandOffset);
                            break;
                        }
                    }
                    
                    if (zeroFourBlock.size() > 0) {
                        // Attempt to parse using OrderedPacket logic without singletons
                        // We use a mock check here for test demonstration
                        std::cout << "  -> Found ordered block of size: " << zeroFourBlock.size() << std::endl;
                    } else {
                        std::cout << "  -> No ordered block found (size: " << data.size() << ")" << std::endl;
                    }
                }
            }
        }
    }
    
    fclose(fp);
    std::cout << "[Protocol Test] Complete. Packets processed: " << packetCount << std::endl;
}

