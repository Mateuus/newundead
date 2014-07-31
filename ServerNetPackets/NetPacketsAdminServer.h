#pragma once

#include "r3dNetwork.h"

namespace NetPacketsAdminServer
{
#pragma pack(push)
#pragma pack(1)

#define ABNET_VERSION		(0x00000004)
#define ABNET_KEY1		0x25e022aa

//
// Game Browser Packet IDs
// 
enum AbpktType_e
{
	PKT_Chatmessage = r3dNetwork::FIRST_FREE_PACKET_ID
};

#ifndef CREATE_PACKET
#define CREATE_PACKET(PKT_ID, VAR) PKT_ID##_s VAR
#endif

struct PKT_Chatmessage_s : public r3dNetPacketMixin<PKT_Chatmessage>
{
   char msg[512];
};

#pragma pack(pop)

}; // namespace NetPacketsGameBrowser