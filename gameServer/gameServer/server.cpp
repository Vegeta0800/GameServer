
#define _WINSOCK_DEPRECATED_NO_WARNINGS //define to use functions from winsock that arent allowed otherwise

//EXTERNAL INCLUDES
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <utility>
#include <unordered_map>
#include <string>
#include <thread>
//INTERNAL INCLUDES
#include "server.h"
#include "database.h"

// link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 12307 //Use any port you want. Has to be in port forwarding settings.
#define DEFAULT_BUFLEN sizeof(LoginData)

//Global variables so the other threads can use their information.
std::vector<Client*> g_clients; //all clients
std::vector<std::pair<Client*, Client*>> g_rooms; //All active rooms
std::unordered_map<std::string, int> g_roomsIndecies; //rooms indecies 

DataBase* dataBase;

//Handels one clients data. Recieves data from one client.
//Then Sends it to all the other clients accordingly.
void ClientSession(Client* client)
{
	int iResult; //Result for recieve functions
	int iSendResult; //Result for send functions
	char buff[DEFAULT_BUFLEN]; //Buffer

	//If room creation or update failed
	bool roomError = false;

	// Receive until the peer shuts down the connection
	do
	{
		//Set buffer to null and iResult to 0
		memset(buff, 0, DEFAULT_BUFLEN);
		iResult = 0;

		//Recieve data from the clientSocket.
		if (!client->loggedIn)
		{
			//If not logged in yet receive login data
			iResult = recv(client->socket, buff, sizeof(LoginData), 0);
		}
		else if(client->loggedIn && !client->inRoom)
		{
			//If logged in but not in room yet receive room data
			iResult = recv(client->socket, buff, sizeof(RoomData), 0);
		}
		else
		{
			//Otherwise receive bools
			iResult = recv(client->socket, buff, 1, 0);
		}

		//If data received isnt a shutdown or an error message
		if (iResult > 0)
		{
			//If client isnt logged in yet
			if (!client->loggedIn)
			{
				//Cast buffer to LoginData
				LoginData loginData = LoginData();
				loginData = *(LoginData*)&buff;

				printf("Recieved: %d bytes from: %s \n", iResult, client->ip.c_str());

				//Database check
				char c[1];

				//Check if login data is registered in database
				if (dataBase->LoginQuery(loginData.name, loginData.password))
				{
					//If so client is logged in
					c[0] = 1;
					client->loggedIn = true;
					client->name = loginData.name;
				}
				else
					c[0] = 0;


				//Send result
				iSendResult = send(client->socket, c, 1, 0);
				printf("Send: %d bytes to: %s \n", iSendResult, client->ip.c_str());

				//Error handling.
				if (iSendResult == SOCKET_ERROR)
				{
					printf("Sending data failed. Error code: %d\n", WSAGetLastError());
					return;
				}

				//If client's login attempt was succesful
				if (client->loggedIn)
				{
					//If there are open rooms
					if (g_rooms.size() != 0)
					{
						//For all rooms
						for (int i = 0; i < g_rooms.size(); i++)
						{
							//Send room data to client
							RoomData data = RoomData();

							//Set room name 
							memset(data.name, 0, 32);
							for (int j = 0; j < 32; j++)
							{
								data.name[j] = g_rooms[i].first->name[j];

								if (g_rooms[i].first->name[j] == '\0') break;
							}

							//Created true
							data.created = true;

							//Send data
							int iSendResult = send(client->socket, (const char*)&data, sizeof(RoomData), 0);
							printf("Send: %d bytes to: %s \n", iSendResult, client->ip.c_str());

							//Error handling.
							if (iSendResult == SOCKET_ERROR)
							{
								printf("Sending data failed. Error code: %d\n", WSAGetLastError());
								return;
							}
						}
					}
				}
			}
			//Else
			else
			{
				//If client isnt in a room yet
				if (!client->inRoom)
				{
					RoomData data = RoomData();
					data = *(RoomData*)&buff;
					printf("Recieved: %d bytes from: %s \n", iResult, client->ip.c_str());

					if (data.deleted)
					{
						for (Client* c : g_clients)
						{
							if (!c->loggedIn) continue;

							iSendResult = send(c->socket, (const char*)&data, sizeof(RoomData), 0);
							printf("Send: %d bytes to: %s \n", iSendResult, c->ip.c_str());
						}

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}
					}

					std::string name = data.name;

					if (data.created)
					{
						g_rooms.push_back(std::make_pair(client, nullptr));
						g_roomsIndecies[name] = (int)(g_rooms.size() - 1);
						client->roomID = (uint8_t)(g_rooms.size() - 1);
						client->host = true;

						printf("%s created a new room!\n", name.c_str());
						client->inRoom = true;
					}
					else
					{
						if (g_rooms[g_roomsIndecies[name]].second == nullptr)
						{
							g_rooms[g_roomsIndecies[name]].second = client;
							printf("%s joined a %s's room!\n", client->name.c_str(), name.c_str());
							client->inRoom = true;
							client->roomID = g_roomsIndecies[name];
						}
						else
						{
							printf("%s's room is full!\n", name.c_str());
							roomError = true;
						}
					}

					if (!roomError)
					{
						for (Client* c : g_clients)
						{
							if (!c->loggedIn) continue;

							if (data.created == true)
								if (c->socket == client->socket) continue;

							iSendResult = send(c->socket, (const char*)&data, sizeof(RoomData), 0);
							printf("Send: %d bytes to: %s \n", iSendResult, c->ip.c_str());

							//Error handling.
							if (iSendResult == SOCKET_ERROR)
							{
								printf("Sending data failed. Error code: %d\n", WSAGetLastError());
								return;
							}
						}
					}

					roomError = false;
				}
				//Otherwise
				else
				{
					//TODO OUT OF GAME

					//Clients ready state is received
					client->ready = buff[0];

					//If both clients in a room are ready
					if (g_rooms[client->roomID].first->ready && g_rooms[client->roomID].second->ready)
					{
						//Create message
						char mess[16];

						//With the joined users IP address
						for (int i = 0; i < 15; i++)
						{
							mess[i] = g_rooms[client->roomID].second->ip[i];

							if (g_rooms[client->roomID].second->ip[i] == '\0') break;
						}

						//And host as true
						mess[15] = 1;

						//And send it to the host
						iSendResult = send(g_rooms[client->roomID].first->socket, mess, 16, 0);
						printf("Send: %d bytes to: %s \n", iSendResult, g_rooms[client->roomID].first->name.c_str());

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}

						//Afterwards 

						//Set message to the hosts IP address
						for (int i = 0; i < 15; i++)
						{
							mess[i] = g_rooms[client->roomID].first->ip[i];

							if (g_rooms[client->roomID].first->ip[i] == '\0') break;
						}

						//And the joined bool
						mess[15] = 0;

						//Send message
						iSendResult = send(g_rooms[client->roomID].second->socket, mess, 16, 0);
						printf("Send: %d bytes to: %s \n", iSendResult, g_rooms[client->roomID].second->name.c_str());

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}

						//Set ingame to true
						g_rooms[client->roomID].first->inGame = true;
						g_rooms[client->roomID].second->inGame = true;
					}
				}
			}
		}
		//If there is a result that isnt connection closing, then recieve it and send to all other clients.
		//If connection is closing
		else if (iResult == 0)
		{
			printf("Connection to %s closing...\n", client->ip.c_str());

			//If connection is closing while in a room and logged in
			if (client->loggedIn && client->inRoom)
			{
				//Delete/Update room client is in
				RoomData data = RoomData();
				data.deleted = true;

				if (client->host)
					data.created = true; //Delete
				else
					data.created = false; //Update

				//Set name of room
				memset(data.name, 0, 32);
				for (int j = 0; j < 32; j++)
				{
					if (client->host)
					{
						data.name[j] = client->name[j];

						if (client->name[j] == '\0') break;
					}
					else
					{
						data.name[j] = g_rooms[client->roomID].first->name[j];

						if (g_rooms[client->roomID].first->name[j] == '\0') break;
					}
				}

				//Send message to all logged in clients that are not this client
				//Send close message to client
				for (Client* c: g_clients)
				{
					if (c != client && c->loggedIn)
					{
						iSendResult = send(c->socket, (const char*)&data, sizeof(RoomData), 0);
						printf("Send: %d bytes to: %s \n", iSendResult, c->name.c_str());

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}
					}
					//If c is the client
					else if (c == client)
					{
						//Send close message to client
						char b = 0;

						iSendResult = send(c->socket, &b, 1	, 0);

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}
					}
				}
			}
		}
		else
		{
			printf("Connection to %s closing...\n", client->ip.c_str());

			//If connection is closing while in a room and logged in
			if (client->loggedIn && client->inRoom)
			{
				//Delete/Update room client is in
				RoomData data = RoomData();
				data.deleted = true;

				if (client->host)
					data.created = true; //Delete
				else
					data.created = false; //Update

				//Set name of room
				memset(data.name, 0, 32);
				for (int j = 0; j < 32; j++)
				{
					if (client->host)
					{
						data.name[j] = client->name[j];

						if (client->name[j] == '\0') break;
					}
					else
					{
						data.name[j] = g_rooms[client->roomID].first->name[j];

						if (g_rooms[client->roomID].first->name[j] == '\0') break;
					}
				}

				//Send message to all logged in clients that are not this client
				//Send close message to client
				for (Client* c : g_clients)
				{
					if (c != client && c->loggedIn)
					{
						iSendResult = send(c->socket, (const char*)&data, sizeof(RoomData), 0);
						printf("Send: %d bytes to: %s \n", iSendResult, c->name.c_str());

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}
					}
					//If c is the client
					else if (c == client)
					{
						//Send close message to client
						char b = 0;

						iSendResult = send(c->socket, &b, 1, 0);

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}
					}
				}
			}
		}
	} while (iResult > 0);

	// Shutdown the clientSocket for sending. Because there wont be any data sent anymore.
	iResult = shutdown(client->socket, SD_SEND);

	//Error handling.
	if (iResult == SOCKET_ERROR)
	{
		printf("Failed to execute shutdown(). Error Code: %d\n", WSAGetLastError());
		closesocket(client->socket);
		return;
	}
	
	//If client was in a room
	if (client->inRoom)
	{
		//Remove clients information from all relevant data

		if (g_roomsIndecies.size() > 1)
			g_roomsIndecies.erase(client->name);
		else
			g_roomsIndecies.clear();

		if (client == g_rooms[client->roomID].second)
			g_rooms[client->roomID].second = nullptr;
		else
		{
			if (g_rooms.size() > 1)
				g_rooms.erase(g_rooms.begin() + client->roomID);
			else
				g_rooms.clear();
		}
	}

	//Close the socket for good.
	closesocket(client->socket);	

	//At last erase client from clients vector
	if (g_clients.size() > 1)
		g_clients.erase(g_clients.begin() + client->clientID);
	else
		g_clients.clear();

	//Delete client
	delete client;
	return;
}


//Startup server
void Server::Startup()
{
	// Initialize Winsock
	WSADATA wsaData;
	int fResult = 0;

	//Initialize WinSock
	fResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	//Error handling.
	if (fResult != 0)
	{
		printf("WSAStartup failed: %d\n", fResult);
		WSACleanup();
		return;
	}

	//Create ListenSocket for clients to connect to.
	this->ListenSocket = INVALID_SOCKET;

	//Create Socket object. 
	this->ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //TCP

	//Error handling.
	if (this->ListenSocket == INVALID_SOCKET)
	{
		printf("Failed to execute socket(). Error Code: %ld\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	//Server sock address information
	sockaddr_in service;
	service.sin_family = AF_INET; //IPv4 Address
	service.sin_addr.s_addr = INADDR_ANY; //Any IP interface of server is connectable (LAN, public IP, etc).
	service.sin_port = htons(DEFAULT_PORT); //Set port.

	// Bind TCP socket.
	fResult = bind(this->ListenSocket, reinterpret_cast<SOCKADDR*>(&service), sizeof(SOCKADDR_IN));

	//Error handling.
	if (fResult == SOCKET_ERROR)
	{
		printf("Failed to execute bind(). Error Code: %d\n", WSAGetLastError());
		closesocket(this->ListenSocket);
		WSACleanup();
		return;
	}

	//Connect database
	dataBase = new DataBase();
	dataBase->Connect();

	//Listen to clients
	this->Listen();
}


//Listening for clients
void Server::Listen()
{
	//Start listening to incoming connections.
	//SOMAXCONN is used so the service can define how many inpending connections can be enqueued.
	int fResult = listen(this->ListenSocket, SOMAXCONN);

	//Error handling.
	if (fResult == SOCKET_ERROR)
	{
		printf("Failed to execute listen(). Error Code: %ld\n", WSAGetLastError());
		closesocket(this->ListenSocket);
		WSACleanup();
		return;
	}

	//Client Sockets
	SOCKET ClientSocket;

	//running server
	bool running = true;

	//Reserve 15 threads for 15 clients
	std::vector<std::thread> threads;

	while (running)
	{
		//Reset the client socket.
		ClientSocket = SOCKET_ERROR;

		//Create local variables for the next incoming client.
		SOCKADDR_IN clientAddress;
		int addrlen = sizeof(clientAddress);

		//As long as there is no new client socket
		while (ClientSocket == SOCKET_ERROR)
		{
			//Accept and save the new client socket and fill the local variables with information.
			ClientSocket = accept(this->ListenSocket, reinterpret_cast<SOCKADDR*>(&clientAddress), &addrlen);
		}

		//If max clients
		if (threads.size() == 15)
		{
			running = false;
			break;
		}

		//Convert the information to a string.
		char* ip = inet_ntoa(clientAddress.sin_addr);
		printf("Connected to: %s \n", ip);

		//Create new client
		Client* client = new Client();
		client->ip = ip;
		client->socket = ClientSocket;

		//Save the client to vector
		client->clientID = (uint8_t)g_clients.size();
		g_clients.push_back(client);

		//Create a new thread for the client and send the client as the argument.
		threads.push_back(std::thread(std::bind(ClientSession, client)));
	}

	//Let all threads join first
	for (int i = 0; i < threads.size(); i++)
	{
		threads[i].join();
	}

	//Clear thread list
	threads.clear();

	//Cleanup the server
	Cleanup();
}
//Server Cleanup
void Server::Cleanup()
{
	//Clear all data lists
	g_rooms.clear();
	g_clients.clear();
	g_roomsIndecies.clear();

	//Disconnect and delete the database
	dataBase->Disconnect();
	delete dataBase;

	//Close Socket and Cleanup.
	closesocket(this->ListenSocket);
	WSACleanup();
}