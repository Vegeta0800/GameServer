
#pragma once
//EXTERNAL INCLUDES
#include <winsock2.h>
#include <string>
//INTERNAL INCLUDES

class DataBase;

//Client struct
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

//Login Data without padding
#pragma pack(push, 1)
struct LoginData
{
	char name[32];
	char password[32];
};
#pragma pack(pop)

//Room Data without padding
#pragma pack(push, 1)
struct RoomData
{
	char name[32];
	bool created;
	bool deleted;
};
#pragma pack(pop)

//Server class
class Server
{
public:
	//Startup server
	void Startup();

private:
	//Listening for clients
	void Listen();
	//Server Cleanup
	void Cleanup();
	SOCKET ListenSocket;
};