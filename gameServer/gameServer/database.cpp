
//EXTERNAL INCLUDES
#include <sstream>
#include <iostream>
#include <algorithm>
//INTERNAL INCLUDES
#include "database.h"

//Connect database 
void DataBase::Connect()
{
	mysql_init(&this->mysql);
	conn = mysql_real_connect(&this->mysql, "localhost", "root", "hEauoRIp0wKFeWfalx7c", "game_database", 3306, 0, 0); //0000 == port.
	if (conn == NULL)
	{
		std::cout << mysql_error(&this->mysql) << std::endl;
		return;
	}
}

//Check if a user is registered in database
bool DataBase::LoginQuery(std::string name, std::string password)
{
	//TODO FILTER FOR INJECTION

	//Prepare query
	std::string query = "SELECT username, password FROM user WHERE username = '";
	query += name;
	query += "'";
	query += "AND password = '";
	query += password;
	query += "'";

	//Execute query
	int query_state = mysql_query(this->conn, query.c_str());
	if (query_state != 0)
	{
		return false;
	}

	//Store result
	MYSQL_RES* result = mysql_store_result(this->conn);
	if (result == NULL)
	{
		std::cout << mysql_error(this->conn) << std::endl;
		return true;
	}

	//And show if possible
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
	{
		printf("%s\n", row[0]);
		mysql_free_result(result);
		return true;
	}

	//free result
	mysql_free_result(result);

	//Return true
	return true;
}

//Disconnect database
void DataBase::Disconnect()
{
	mysql_close(this->conn);
}
