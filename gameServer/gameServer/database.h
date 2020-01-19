
#pragma once
//EXTERNAL INCLUDES
#include <mysql.h>
#include <string>
//INTERNAL INCLUDES

class DataBase
{
public:
	//Connect database 
	void Connect();

	//Check if a user is registered in database
	bool LoginQuery(std::string name, std::string password);

	//Disconnect database
	void Disconnect();
private:
	MYSQL mysql;
	MYSQL* conn;

};