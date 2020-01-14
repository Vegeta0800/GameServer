#pragma once
#include <winsock2.h>
#include <string>

struct Client
{
	SOCKET socket;
	std::string ip;
	std::string name;
	uint8_t roomID;
	uint8_t clientID;
	bool ready = false;
	bool inRoom = false;
	bool host = false;
	bool loggedIn = false;
	bool inGame = false;
};

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
	bool deleted;
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