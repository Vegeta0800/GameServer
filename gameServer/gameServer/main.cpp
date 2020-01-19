
//EXTERNAL INCLUDES
//INTERNAL INCLUDES
#include "server.h"

int main()
{
	//Create server
	Server* server = new Server();

	//Start server
	server->Startup();

	//Delete server when finished
	delete server;

	return 0;
}