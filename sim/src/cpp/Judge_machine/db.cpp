#include <cstdio>
#include <cstring>
#include "db.hpp"

DB::DB(): con(), user(NULL), password(NULL), database(NULL)
{
	FILE *f = fopen("../php/db.pass", "r");
	if(f == NULL)
	{
		printf("#1\n");
		exit(1);
	}
	size_t x;
	if(getline(&user, &x, f) == -1 || getline(&password, &x, f) == -1 || getline(&database, &x, f) == -1)
	{
		printf("%p\n%p\n%p\n", user, password, database);
		exit(1);
	}
	fclose(f);
	user[strlen(user)-1] = password[strlen(password)-1] = database[strlen(database)-1] = '\0';
#ifdef DEBUG
	printf("mysql_user: %s\nmysql_password: %s\ndatabase: %s\n", user, password, database);
#endif
	connect();
}

void DB::connect()
{
	if(con)
		delete con;
	con = get_driver_instance()->connect("localhost", user, password);
	con->setSchema(database);
}

DB DB::obj;
