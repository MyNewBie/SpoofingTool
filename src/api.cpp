#include "stdafx.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "api.h"

enum
{
	PACKER_BUFFER_SIZE = 1024 * 2
};
unsigned char m_aBuffer[PACKER_BUFFER_SIZE];

unsigned char *m_pCurrent;
unsigned char *m_pEnd;
int m_Error;

int Sequence = 0;// 0 - 1023

unsigned char* CompressionPack(unsigned char *pDst, int i)
{
	*pDst = (i >> 25) & 0x40; // set sign bit if i<0
	i = i ^ (i >> 31); // if(i<0) i = ~i

	*pDst |= i & 0x3F; // pack 6bit into dst
	i >>= 6; // discard 6 bits
	if (i)
	{
		*pDst |= 0x80; // set extend bit
		while (1)
		{
			pDst++;
			*pDst = i&(0x7F); // pack 7bit
			i >>= 7; // discard 7 bits
			*pDst |= (i != 0) << 7; // set extend bit (may branch)
			if (!i)
				break;
		}
	}

	pDst++;
	return pDst;
}

void Reset()
{
	m_Error = 0;
	m_pCurrent = m_aBuffer;
	m_pEnd = m_pCurrent + PACKER_BUFFER_SIZE;
}

void AddInt(int i)
{
	if (m_Error)
		return;

	// make sure that we have space enough
	if (m_pEnd - m_pCurrent < 6)
	{
		m_Error = 1;
	}
	else
		m_pCurrent = CompressionPack(m_pCurrent, i);
}

void AddString(const char *pStr, int Limit)
{
	if (m_Error)
		return;

	//
	if (Limit > 0)
	{
		while (*pStr && Limit != 0)
		{
			*m_pCurrent++ = *pStr++;
			Limit--;

			if (m_pCurrent >= m_pEnd)
			{
				m_Error = 1;
				break;
			}
		}
		*m_pCurrent++ = 0;
	}
	else
	{
		while (*pStr)
		{
			*m_pCurrent++ = *pStr++;

			if (m_pCurrent >= m_pEnd)
			{
				m_Error = 1;
				break;
			}
		}
		*m_pCurrent++ = 0;
	}
}

int Size() { return (int)(m_pCurrent - m_aBuffer); }
const unsigned char *Data() { return m_aBuffer; }

enum
{
	NETMSG_NULL = 0,

	// the first thing sent by the client
	// contains the version info for the client
	NETMSG_INFO = 1,

	// sent by server
	NETMSG_MAP_CHANGE,		// sent when client should switch map
	NETMSG_MAP_DATA,		// map transfer, contains a chunk of the map file
	NETMSG_CON_READY,		// connection is ready, client should send start info
	NETMSG_SNAP,			// normal snapshot, multiple parts
	NETMSG_SNAPEMPTY,		// empty snapshot
	NETMSG_SNAPSINGLE,		// ?
	NETMSG_SNAPSMALL,		//
	NETMSG_INPUTTIMING,		// reports how off the input was
	NETMSG_RCON_AUTH_STATUS,// result of the authentication
	NETMSG_RCON_LINE,		// line that should be printed to the remote console

	NETMSG_AUTH_CHALLANGE,	//
	NETMSG_AUTH_RESULT,		//

	// sent by client
	NETMSG_READY,			//
	NETMSG_ENTERGAME,
	NETMSG_INPUT,			// contains the inputdata from the client
	NETMSG_RCON_CMD,		//
	NETMSG_RCON_AUTH,		//
	NETMSG_REQUEST_MAP_DATA,//

	NETMSG_AUTH_START,		//
	NETMSG_AUTH_RESPONSE,	//

	// sent by both
	NETMSG_PING,
	NETMSG_PING_REPLY,
	NETMSG_ERROR,

	// sent by server (todo: move it up)
	NETMSG_RCON_CMD_ADD,
	NETMSG_RCON_CMD_REM,
};

enum
{
	NETMSG_INVALID = 0,
	NETMSGTYPE_SV_MOTD,
	NETMSGTYPE_SV_BROADCAST,
	NETMSGTYPE_SV_CHAT,
	NETMSGTYPE_SV_KILLMSG,
	NETMSGTYPE_SV_SOUNDGLOBAL,
	NETMSGTYPE_SV_TUNEPARAMS,
	NETMSGTYPE_SV_EXTRAPROJECTILE,
	NETMSGTYPE_SV_READYTOENTER,
	NETMSGTYPE_SV_WEAPONPICKUP,
	NETMSGTYPE_SV_EMOTICON,
	NETMSGTYPE_SV_VOTECLEAROPTIONS,
	NETMSGTYPE_SV_VOTEOPTIONLISTADD,
	NETMSGTYPE_SV_VOTEOPTIONADD,
	NETMSGTYPE_SV_VOTEOPTIONREMOVE,
	NETMSGTYPE_SV_VOTESET,
	NETMSGTYPE_SV_VOTESTATUS,
	NETMSGTYPE_CL_SAY,
	NETMSGTYPE_CL_SETTEAM,
	NETMSGTYPE_CL_SETSPECTATORMODE,
	NETMSGTYPE_CL_STARTINFO,
	NETMSGTYPE_CL_CHANGEINFO,
	NETMSGTYPE_CL_KILL,
	NETMSGTYPE_CL_EMOTICON,
	NETMSGTYPE_CL_VOTE,
	NETMSGTYPE_CL_CALLVOTE,
	NETMSGTYPE_CL_ISDDNET,
	NETMSGTYPE_SV_DDRACETIME,
	NETMSGTYPE_SV_RECORD,
	NETMSGTYPE_SV_PLAYERTIME,
	NETMSGTYPE_SV_TEAMSSTATE,
	NETMSGTYPE_CL_SHOWOTHERS,
	NUM_NETMSGTYPES
};

enum
{
	FLAGS_VITAL = 0,
	FLAGS_FLUSH
};

unsigned char* PackHeader(unsigned char *pData, int m_Flags, int m_Size, int m_Sequence, int flags)
{
	pData[0] = ((m_Flags & 3) << 6) | ((m_Size >> 4) & 0x3f);
	pData[1] = (m_Size & 0xf);
	if (flags&FLAGS_VITAL)
	{
		pData[1] |= (m_Sequence >> 2) & 0xf0;
		pData[2] = m_Sequence & 0xff;

		return pData + 3;
	}
	return pData + 2;
}

int StartofPacking(unsigned char *buffer, int flags)
{
	int Flags = 0;
	//int Sequence = 0;// 0 - 1023
	int Ack = 0;

	Flags &= ~8; // NO COMMPRESSION FLAG CUZ IT SUCKZ

	int BufferSize = 0;

	buffer[0] = ((Flags << 4) & 0xf0) | ((Ack >> 8) & 0xf);
	buffer[1] = Ack & 0xff;
	buffer[2] = 1;//--ChunkNum

	BufferSize += 3;

	//space for the header
	if (flags&FLAGS_VITAL)
	{
		BufferSize += 3;
		Sequence = (Sequence + 1) % 1024;
	}
	else
		BufferSize += 2;

	Reset();
	
	return BufferSize;
}

int EndofPacking(unsigned char *buffer, int buffersize, int flags)
{
	m_aBuffer[0] <<= 1; // shift the packet id
	memcpy(&buffer[buffersize], m_aBuffer, Size());

	buffersize += Size();

	//printf("%d", sizebefore);
	int sizebefore = buffersize - Size() - 2;
	PackHeader(&buffer[sizebefore], 0, Size(), Sequence, flags); // the header 

	return buffersize;
}

int PackSay(unsigned char *buffer, char *message, int team)
{
	int BufferSize = StartofPacking(buffer, FLAGS_FLUSH);

	AddInt(NETMSGTYPE_CL_SAY); //--packet id
	AddInt(team); //--team
	AddString(message, -1); //--text
	
	return EndofPacking(buffer, BufferSize, FLAGS_FLUSH);
}

int PackConnect(unsigned char *buffer)
{
	strcpy_s((char *)buffer, 4, "\x10\x00\x00\x01");
	return 4;
}

int PackClientInfo(unsigned char *buffer)
{
	int BufferSize = StartofPacking(buffer, FLAGS_VITAL | FLAGS_FLUSH);

	AddInt(NETMSG_INFO); //--packet id
	AddString("0.6 626fce9a778df4d4", 128);// GAME_NETVERSION "0.6 626fce9a778df4d4"
	AddString("", 128); // password (to teh server?)

	return EndofPacking(buffer, BufferSize, FLAGS_VITAL|FLAGS_FLUSH);
}

int PackReady(unsigned char *buffer)
{
	int BufferSize = StartofPacking(buffer, FLAGS_VITAL | FLAGS_FLUSH);

	AddInt(NETMSG_READY); //--packet id

	return EndofPacking(buffer, BufferSize, FLAGS_VITAL | FLAGS_FLUSH);
}

int PackEnterGame(unsigned char *buffer)
{
	int BufferSize = StartofPacking(buffer, FLAGS_VITAL | FLAGS_FLUSH);

	AddInt(NETMSG_ENTERGAME); //--packet id

	return EndofPacking(buffer, BufferSize, FLAGS_VITAL | FLAGS_FLUSH);
}

int PackSendInfo(unsigned char *buffer)
{
	int BufferSize = StartofPacking(buffer, FLAGS_FLUSH);

	AddInt(NETMSGTYPE_CL_STARTINFO);
	AddString("fgt", -1);
	AddString("", -1);
	AddInt(-1);
	AddString("default", -1);
	AddInt(0);
	AddInt(65048);
	AddInt(65048);

	return EndofPacking(buffer, BufferSize, FLAGS_FLUSH);	
}

int PackRconAuth(unsigned char *buffer)
{
	int BufferSize = StartofPacking(buffer, FLAGS_FLUSH);

	AddInt(NETMSG_RCON_AUTH);
	AddString("Metzgerlin", 32);
	AddString("wrongpw", 32);
	AddInt(1);

	return EndofPacking(buffer, BufferSize, FLAGS_FLUSH);	
}