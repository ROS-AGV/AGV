 /***********************************************************************************************
 * SOFTWARE RSL400
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE
 *
 * Description:
 * UDP Stateimage of RSL400
 *
 **************************************************************************************************/

#pragma once

#include "stateimage.h"
#include "math.h"

#pragma pack(1)

typedef unsigned short WORD;
typedef unsigned int DWORD;

namespace RSL400 {
	// Header1
	typedef struct tagHeader1
	{
		DWORD TotalSize;
		unsigned char Size;
		unsigned char FollowFlag;
		WORD RegID;
	}
	Header1, *PHeader1;

	// Header2
	typedef struct tagHeader2
	{
		unsigned char Size;
		unsigned char Destination;
		unsigned char Source;
		unsigned char TelegramType;

	}
	Header2, *PHeader2;

	// UDP Telegram Type
	typedef struct tagUdpTelegramType
	{
		Header1 H1;
		Header2 H2;
		WORD Id;
		WORD BlockNo;
		DWORD ScanNo;
	}
	UdpTelegramType, *PUdpTelegramType;

	// Beam Description
	typedef struct tagBeamDescription 
	{
		WORD Start;
		WORD Stop;
		WORD Resolution;
		WORD Res;
	}
	BeamDescription, *PBeamDescription;

	// UdpStateImageType1
	typedef struct tagUdpStateImageType1 : UdpTelegramType
	{
		StateImageType1 StateImage1;
	}
	UdpStateImageType1, *PUdpStateImageType1;

	// UdpExtStateImageType1
	typedef struct tagUdpExtStateImageType1 : tagUdpStateImageType1
	{
		BeamDescription BeamDesc;
	}
	UdpExtStateImageType1, *PUdpExtStateImageType1;

	// Single Beam
	typedef struct tagBeam
	{
		WORD Distance;		// distance
	}
	Beam, *PBeam;

	// UdpBeamPacket
	typedef struct tagUdpBeamPacket : UdpTelegramType
	{
		Beam Beams[1];
	}
	UdpBeamPacket, *PUdpBeamPacket;

	/// <summary>
	/// Gets the beam count.
	/// </summary>
	/// <param name="pBeamDesc">The beam description.</param>
	/// <returns>The beam count according to beam description.</returns>
	int getBeamCount( PBeamDescription pBeamDesc ) {
		if ( pBeamDesc->Start == 0 && pBeamDesc->Stop == 0 && pBeamDesc->Resolution == 0 )
        {
            return 0;
        }

        return 1 + (int)ceil((pBeamDesc->Stop - pBeamDesc->Start)/(double)pBeamDesc->Resolution);
	}

}

#pragma pack()
