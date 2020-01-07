#pragma once
#include <winsock2.h>
#include <string>

#pragma pack(push, 1)
struct LoginData
{
	char name[32];
	char password[32];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RoomData
{
	char name[32];
	bool created;
};
#pragma pack(pop)

class Server
{
public:
	void Startup();

private:
	void Listen();
	void Cleanup();
	SOCKET ListenSocket;
};