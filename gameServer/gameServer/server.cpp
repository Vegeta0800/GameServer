#define _WINSOCK_DEPRECATED_NO_WARNINGS

//EXTERNAL INCLUDES
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <string>
//INTERNAL INCLUDES
#include "server.h"

// link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 12307 //Use any port you want. Has to be in port forwarding settings.
#define DEFAULT_BUFLEN sizeof(LoginData)

//Global variables so the other threads can use their information.
std::vector<SOCKET> g_sockets;
std::vector<SOCKET> g_loggedSockets;
std::vector<char*> g_rooms;
std::unordered_map<SOCKET, char*> g_ips;

std::unordered_map<SOCKET, char*> onlineClients;

std::unordered_map<char*, uint8_t> rooms;

//Handels one clients data. Recieves data from one client.
//Then Sends it to all the other clients.
DWORD WINAPI ClientSession(LPVOID lpParameter)
{
	//Parameter conversion
	SOCKET ClientSocket = (SOCKET)lpParameter;

	//Recieve and Send Data
	int iResult; //Bytes recieved
	int iSendResult;
	char buff[DEFAULT_BUFLEN];

	bool loggedIn = false;
	bool inRoom = false;

	bool roomError = false;

	// Receive until the peer shuts down the connection
	do
	{
		//Recieve data from the clientSocket.
		if (!loggedIn)
			iResult = recv(ClientSocket, buff, sizeof(LoginData), 0);
		else if (loggedIn && !inRoom)
			iResult = recv(ClientSocket, buff, sizeof(RoomData), 0);

		if (iResult > 0)
		{
			if (!loggedIn)
			{
				LoginData loginData = LoginData();
				loginData = *(LoginData*)&buff;

				printf("Recieved: %d bytes from: %s \n", iResult, g_ips[ClientSocket]);
				printf("Name: %s, Password: %s\n", loginData.name, loginData.password);

				//INSERT DATABASE CHECK
				char c[1];
				c[0] = 1;

				loggedIn = true;
				onlineClients[ClientSocket] = loginData.name;
				g_loggedSockets.push_back(ClientSocket);

				iSendResult = send(ClientSocket, c, 1, 0);
				printf("Send: %d bytes to: %s \n", iSendResult, g_ips[ClientSocket]);

				//Error handling.
				if (iSendResult == SOCKET_ERROR)
				{
					printf("Sending data failed. Error code: %d\n", WSAGetLastError());
					return 0;
				}

				if (g_rooms.size() != 0)
				{
					for (int i = 0; i < g_rooms.size(); i++)
					{
						RoomData data = RoomData();

						memset(data.name, 0, 32);
						for (int j = 0; j < 32; j++)
						{
							data.name[j] = g_rooms[i][j];
						}

						data.created = true;

						int iSendResult = send(ClientSocket, (const char*)&data, sizeof(RoomData), 0);
						printf("Send: %d bytes to: %s \n", iSendResult, g_ips[ClientSocket]);

						//Error handling.
						if (iSendResult == SOCKET_ERROR)
						{
							printf("Sending data failed. Error code: %d\n", WSAGetLastError());
							return 0;
						}
					}
				}
			}
			else
			{
				if (!inRoom)
				{
					RoomData data = RoomData();
					data = *(RoomData*)&buff;

					printf("Recieved: %d bytes from: %s \n", iResult, g_ips[ClientSocket]);

					if (data.created)
					{
						rooms[data.name] = 1;
						g_rooms.push_back(data.name);
						printf("%s created a new room!\n", data.name);
					}
					else
					{
						if (rooms[data.name] == 1)
						{
							rooms[data.name] = 2;
							printf("%s joined a %s's room!\n", onlineClients[ClientSocket], data.name);
						}
						else
						{
							printf("%s's room is full!\n", onlineClients[ClientSocket]);
							roomError = true;
						}
					}

					if (!roomError)
					{
						for (SOCKET s : g_loggedSockets)
						{
							if (s != ClientSocket)
							{
								iSendResult = send(s, buff, iResult, 0);
								printf("Send: %d bytes to: %s \n", iSendResult, g_ips[ClientSocket]);

								//Error handling.
								if (iSendResult == SOCKET_ERROR)
								{
									printf("Sending data failed. Error code: %d\n", WSAGetLastError());
									return 0;
								}
							}
						}

						inRoom = true;
					}

					roomError = false;
				}
			}
		}
		//If there is a result that isnt connection closing, then recieve it and send to all other clients.
		//If connection is closing
		else if (iResult == 0)
			printf("Connection to %s closing...\n", g_ips[ClientSocket]);
	} while (iResult > 0);

	// Shutdown the clientSocket for sending. Because there wont be any data sent anymore.
	iResult = shutdown(ClientSocket, SD_SEND);

	//Error handling.
	if (iResult == SOCKET_ERROR)
	{
		printf("Failed to execute shutdown(). Error Code: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}

	//Close the socket for good.
	closesocket(ClientSocket);

	//At last remove the socket from the active sockets vector.
	g_sockets.erase(std::remove(g_sockets.begin(), g_sockets.end(), ClientSocket), g_sockets.end());
	return 0;
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

	Listen();
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

		//Save the socket and ip to the global variables.
		g_sockets.push_back(ClientSocket);
		g_ips[ClientSocket] = ip;

		//Create a new thread for the client and send the client as the argument.
		DWORD dwThreadId;
		CreateThread(NULL, 0, ClientSession, (LPVOID)ClientSocket, 0, &dwThreadId);
	}

	Cleanup();
}

void Server::Cleanup()
{
	//Close Socket and Cleanup.
	closesocket(this->ListenSocket);
	WSACleanup();
}
