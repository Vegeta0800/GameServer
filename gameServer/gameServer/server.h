#pragma once
#include <winsock2.h>
#include <string>

#pragma pack(push, 1)
struct Data
{
	std::string data;
	int ID;
};
#pragma pack(pop)

//ID 0
#pragma pack(push, 1)
struct LoginData
{
	std::string name;
	std::string password;
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