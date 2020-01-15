#include "database.h"
#include <sstream>
#include <iostream>
#include <algorithm>

void DataBase::Connect()
{
	mysql_init(&this->mysql);
	conn = mysql_real_connect(&this->mysql, "server", "name", "password", "scheme", 0000, 0, 0); //0000 == port.
	if (conn == NULL)
	{
		std::cout << mysql_error(&this->mysql) << std::endl;
		return;
	}
}

bool DataBase::LoginQuery(std::string name, std::string password)
{
	std::string query = "SELECT username, password FROM user WHERE username = '";
	query += name;
	query += "'";

	query += "AND password = '";
	query += password;
	query += "'";

	int query_state = mysql_query(this->conn, query.c_str());
	if (query_state != 0)
	{
		return false;
	}

	MYSQL_RES* result = mysql_store_result(this->conn);

	if (result == NULL)
	{
		std::cout << mysql_error(this->conn) << std::endl;
		return false;
	}

	MYSQL_ROW row;

	if (row = mysql_fetch_row(result))
	{
		printf("%s\n", row[0]);
		return true;
	}

	mysql_free_result(result);

	return false;
}

std::string DataBase::Hash(std::string input)
{
	unsigned int magic = 397633456;
	unsigned int hash = 729042348;

	for (int i = 0; i < input.length(); i++)
	{
		hash = hash ^ (input[i]);
		hash = hash * magic;
	}

	std::string hexHash;
	std::stringstream hexStream;

	hexStream << std::hex << hash;
	hexHash = hexStream.str();

	std::transform(hexHash.begin(), hexHash.end(), hexHash.begin(), ::toupper);
	
	return hexHash;
}

void DataBase::Disconnect()
{
	mysql_close(this->conn);
}
