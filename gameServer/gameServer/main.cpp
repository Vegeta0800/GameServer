
#include "server.h"

int main()
{
	Server* server = new Server();

	server->Startup();

	delete server;

	return 0;
}