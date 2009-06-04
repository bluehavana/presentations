#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "spawn.c"

int main( int argc, char **argv)
{
	int my_stdin = 0, my_stdout = 0, my_stderr = 0;
	pid_t child_pid;
	ssize_t bytes_read;
	char *command = { "/usr/bin/echo" };
	char *buffer[1024];
	
	if ( argv[1] )
	{
		spawn( command, argv, NULL, &my_stdin, &my_stdout, &my_stderr, &child_pid );
	}
	
	bytes_read = read( my_stdout, buffer, 1023 );
	
	printf( "read return value: %d\n", bytes_read );
	printf( "%s\n", buffer );
	
	bytes_read = read( my_stderr, buffer, 1023 );
	fprintf( stderr, "Bytes read from stderr: %d\n", bytes_read );
	fprintf( stderr, "%s\n", buffer );
	
	return 0;
}