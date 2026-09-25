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

#include "TCPVarLenSocket.h"
#include "Common.h"

TCPVarLenSocket::TCPVarLenSocket(ISocketHandler& h) : TcpSocket(h)
{
}

TCPVarLenSocket::~TCPVarLenSocket()
{
}


void TCPVarLenSocket::OnRead()
{
	// OnRead of TcpSocket actually reads the data from the socket
	// and moves it to the input buffer (ibuf)
	TcpSocket::OnRead();
	// get number of bytes in input buffer
	size_t n = ibuf.GetLength();
	if (n == 0)
		return;

	// Edge crawler protection: drop HTTP, TLS, SSLv2, SSH probes immediately with zero logging
	char peekBuf[8] = { 0 };
	size_t peekLen = (n < sizeof(peekBuf)) ? n : sizeof(peekBuf);
	if (peekLen >= 2 && ibuf.Peek(peekBuf, peekLen))
	{
		const unsigned char* u = reinterpret_cast<const unsigned char*>(peekBuf);
		bool dropCrawler = false;

		// HTTP methods (3 chars) & SSH probe
		if (peekLen >= 3)
		{
			if (memcmp(peekBuf, "GET", 3) == 0 ||
			    memcmp(peekBuf, "POS", 3) == 0 ||
			    memcmp(peekBuf, "HEA", 3) == 0 ||
			    memcmp(peekBuf, "PUT", 3) == 0 ||
			    memcmp(peekBuf, "DEL", 3) == 0 ||
			    memcmp(peekBuf, "OPT", 3) == 0 ||
			    memcmp(peekBuf, "CON", 3) == 0 ||
			    memcmp(peekBuf, "TRA", 3) == 0 ||
			    memcmp(peekBuf, "PAT", 3) == 0 ||
			    memcmp(peekBuf, "PRI", 3) == 0 ||
			    memcmp(peekBuf, "SSH", 3) == 0)
			{
				dropCrawler = true;
			}
		}

		// TLS record header: ContentType 0x14..0x17 followed by version 0x03 (SSLv3 / TLS 1.0 - 1.3)
		if (!dropCrawler && peekLen >= 2)
		{
			if ((u[0] >= 0x14 && u[0] <= 0x17) && u[1] == 0x03)
			{
				dropCrawler = true;
			}
		}

		// SSLv2 ClientHello probe:
		// Starts with 2-byte header with MSB set ((u[0] & 0x80) != 0).
		// Byte 2 is msg_type 0x01 (CLIENT_HELLO).
		// Note: Matrix Online CERT_ConnectRequest packet has u[0]=0x81, u[2]=0x01, u[3]=0x03, u[4]=0x00, u[5]=0x36.
		// To avoid dropping valid CERT_ConnectRequest:
		// Check for SSLv2 version (u[3]==0x00 && u[4]==0x02) or TLS version (u[3]==0x03 && u[4]>=0x01 && u[4]<=0x03),
		// or SSLv3 hello (u[3]==0x03 && u[4]==0x00 && peekLen >= 6 && u[5]==0x00).
		if (!dropCrawler && (u[0] & 0x80) != 0 && peekLen >= 5 && u[2] == 0x01)
		{
			if ((u[3] == 0x00 && u[4] == 0x02) ||
			    (u[3] == 0x03 && u[4] >= 0x01 && u[4] <= 0x03) ||
			    (u[3] == 0x03 && u[4] == 0x00 && peekLen >= 6 && u[5] == 0x00))
			{
				dropCrawler = true;
			}
		}

		if (dropCrawler)
		{
			SetCloseAndDelete(true);
			return;
		}
	}

	while (n >= 2)
	{
		byte firstTwoBytes[2];
		//peek first 2 bytes
		ibuf.Peek((char*)&firstTwoBytes,2);

		int sizeOfPacketSize = 1;
		if (firstTwoBytes[0] > 0x7F)
		{
			sizeOfPacketSize = 2;
			firstTwoBytes[0] -= 0x80;
		}

		uint16 packetSize = 0;
		if (sizeOfPacketSize == 1)
		{
			packetSize = firstTwoBytes[0];
		}
		else if (sizeOfPacketSize == 2)
		{
			memcpy(&packetSize,firstTwoBytes,sizeof(packetSize));
			packetSize = swap16(packetSize);
		}

		// Security: Prevent OOM exploits via massive spoofed packet sizes
		if (packetSize > 8192)
		{
			SetCloseAndDelete();
			return;
		}

		n = ibuf.GetLength();
		size_t requiredLen = sizeOfPacketSize+packetSize;
		if (n >= requiredLen)
		{
			ibuf.Remove(sizeOfPacketSize);

			vector<byte> tempStorage;
			tempStorage.resize(packetSize);
			ibuf.Read((char*)&tempStorage[0],tempStorage.size());

			ProcessData(&tempStorage[0],tempStorage.size());
			
			// Update buffer size for next loop iteration
			n = ibuf.GetLength();
		}
		else
		{
			// Need more data
			break;
		}
	}
}

void TCPVarLenSocket::SendPacket( const TCPVariableLengthPacket &varLenPacket )
{
	ByteBuffer withHeader = varLenPacket.GetProcessed();
	SendBuf(withHeader.contents(),withHeader.size());
}
