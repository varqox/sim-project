#include <cstdio>
#include <cstring>
#include "db.hpp"

DB::DB(): con()
{
	FILE *f = fopen("../php/db.pass", "r");
	if(f == NULL)
	{
		printf("#1\n");
		exit(1);
	}
	char *user = NULL, *password = NULL, *database = NULL;
	size_t x;
	if(getline(&user, &x, f) == -1 || getline(&password, &x, f) == -1 || getline(&database, &x, f) == -1)
	{
		printf("%p\n%p\n%p\n", user, password, database);
		exit(1);
	}
	user[strlen(user)-1] = password[strlen(password)-1] = database[strlen(database)-1] = '\0';
	printf("%s\n%s\n%s\n", user, password, database);
	con = get_driver_instance()->connect("localhost", user, password);
	con->setSchema(database);
	free(user);
	free(password);
	free(database);
	fclose(f);
}

DB DB::obj;
