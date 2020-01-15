
#pragma once
//EXTERNAL INCLUDES
#include <mysql.h>
#include <string>


class DataBase
{
public:
	void Connect();

	bool LoginQuery(std::string name, std::string password);
	std::string Hash(std::string input);

	void Disconnect();
private:
	MYSQL mysql;
	MYSQL* conn;

};