#pragma once
#include <winsock2.h>
#include <string>

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