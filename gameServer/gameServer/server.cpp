#define _WINSOCK_DEPRECATED_NO_WARNINGS

//EXTERNAL INCLUDES
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <utility>
#include <unordered_map>
#include <string>
#include <thread>

#include "server.h"
#include "database.h"

// link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 12307 //Use any port you want. Has to be in port forwarding settings.
#define DEFAULT_BUFLEN sizeof(LoginData)

//Global variables so the other threads can use their information.

std::vector<Client*> g_clients;
std::vector<std::pair<Client*, Client*>> g_rooms;
std::unordered_map<std::string, int> g_roomsIndecies;

DataBase* dataBase;

//Handels one clients data. Recieves data from one client.
//Then Sends it to all the other clients.
void ClientSession(Client* client)
{
	//Recieve and Send Data
	int iResult; //Bytes recieved
	int iSendResult;
	char buff[DEFAULT_BUFLEN];

	bool roomError = false;

	// Receive until the peer shuts down the connection
	do
	{
		memset(buff, 0, DEFAULT_BUFLEN);
		iResult = 0;

		//Recieve data from the clientSocket.
		if (!client->loggedIn)
		{
			iResult = recv(client->socket, buff, sizeof(LoginData), 0);
		}
		else if(client->loggedIn && !client->inRoom)
		{
			iResult = recv(client->socket, buff, sizeof(RoomData), 0);
		}
		else
		{
			iResult = recv(client->socket, buff, 1, 0);
		}

		if (iResult > 0)
		{
			if (!client->loggedIn)
			{
				LoginData loginData = LoginData();
				loginData = *(LoginData*)&buff;

				printf("Recieved: %d bytes from: %s \n", iResult, client->ip.c_str());

				//Database check
				char c[1];
				
				if (dataBase->LoginQuery(loginData.name, loginData.password))
				{
					c[0] = 1;
					client->loggedIn = true;
					client->name = loginData.name;
				}
				else
					c[0] = 0;


				iSendResult = send(client->socket, c, 1, 0);
				printf("Send: %d bytes to: %s \n", iSendResult, client->ip.c_str());

				//Error handling.
				if (iSendResult == SOCKET_ERROR)
				{
					printf("Sending data failed. Error code: %d\n", WSAGetLastError());
					return;
				}

				if (c[0] == 1)
				{
					if (g_rooms.size() != 0)
					{
						for (int i = 0; i < g_rooms.size(); i++)
						{
							RoomData data = RoomData();

							memset(data.name, 0, 32);
							for (int j = 0; j < 32; j++)
							{
								data.name[j] = g_rooms[i].first->name[j];

								if (g_rooms[i].first->name[j] == '\0') break;
							}

							data.created = true;

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
			else
			{
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
				else
				{
					client->ready = buff[0];
					if (g_rooms[client->roomID].first->ready && g_rooms[client->roomID].second->ready)
					{
						char mess[16];

						int j = 0;
						for (int i = 0; i < 15; i++)
						{
							mess[i] = g_rooms[client->roomID].first->ip[i];
							j++;

							if (g_rooms[client->roomID].first->ip[i] == '\0') break;
						}
						j++;
						mess[j] = 1;

						iSendResult = send(g_rooms[client->roomID].first->socket, mess, (j + 1), 0);
						printf("Send: %d bytes to: %s \n", iSendResult, g_rooms[client->roomID].first->name.c_str());

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}

						mess[j] = 0;

						iSendResult = send(g_rooms[client->roomID].second->socket, mess, (j + 1), 0);
						printf("Send: %d bytes to: %s \n", iSendResult, g_rooms[client->roomID].second->name.c_str());

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return;
						}

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

			if (client->loggedIn && client->inRoom)
			{
				RoomData data = RoomData();
				data.deleted = true;

				if (client->host)
					data.created = true;
				else
					data.created = false;

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
					else if (c == client)
					{
						char b = 0;

						iSendResult = send(c->socket, &b, 1	, 0);
						printf("Closing connection with %s \n", c->name.c_str());

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
	
	if (client->inRoom)
	{
		//At last remove the socket from the active sockets vector.
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

	if (g_clients.size() > 1)
		g_clients.erase(g_clients.begin() + client->clientID);
	else
		g_clients.clear();

	delete client;

	return;
}

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
	this->ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Error handling.
	if (this->ListenSocket == INVALID_SOCKET)
	{
		printf("Failed to execute socket(). Error Code: %ld\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	sockaddr_in service;
	service.sin_family = AF_INET; //Ipv4 Address
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

	dataBase = new DataBase();
	dataBase->Connect();

	this->Listen();
}

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

	bool running = true;

	std::vector<std::thread> threads;
	threads.reserve(10);

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

		//Convert the information to a string.
		char* ip = inet_ntoa(clientAddress.sin_addr);
		printf("Connected to: %s \n", ip);

		Client* client = new Client();
		client->ip = ip;
		client->socket = ClientSocket;

		//Save the socket and ip to the global variables.
		client->clientID = (uint8_t)g_clients.size();
		g_clients.push_back(client);

		//Create a new thread for the client and send the client as the argument.
		threads.push_back(std::thread(std::bind(ClientSession, client)));
	}

	for (int i = 0; i < threads.size(); i++)
	{
		threads[i].join();
	}

	threads.clear();

	Cleanup();
}

void Server::Cleanup()
{
	g_rooms.clear();
	g_clients.clear();
	g_roomsIndecies.clear();

	dataBase->Disconnect();
	delete dataBase;

	//Close Socket and Cleanup.
	closesocket(this->ListenSocket);
	WSACleanup();
}
