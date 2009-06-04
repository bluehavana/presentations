/*
* License: Use anyway you wish, but an acknowledgement as the author would be much appreciated,
* especially if in the same room and being present to others.  All my own code, but definitely
* helped out by the web.
*
* Author: Jeremiah McCann (for the June San Antonio Hackers Association meeting)
*
* Description:  Launches command in executable_path with full access to stdin, stdout and stderr.
* Uses execve to copy arguments to argv of target memory space to avoid shell expansion command
* line injection.
*
* http:
*/

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifndef _spawn_c
#define _spawn_c

/* These two functions are used internally. */
static int child_redirect_pipes( const int *my_stdin, const int *my_stdout, const int *my_stderr );

static int setup_pipes( const int *my_stdin, const int *my_stdout, const int *my_stderr );

/*
* int spawn( const char *executable_path, char *const args[], char *const env[],
*			int *arg_stdin_fd, int *arg_stdout_fd, int *arg_stderr_fd,
*			pid_t *child_pid )
*
* Spawns a process through fork/execve with access to the file descriptors specified by
* arg_stdin_fd, arg_stdout_fd and arg_stderr_fd.  You write to arg_stdin_fd to write to the process'
* stdin and read from arg_stdout_fd/arg_stderr_fd to read from those file descriptors.
* Only tested in Cygwin with compile errors.  Not production ready, needs about 10 more refactorings
* to be production ready.
*
*/

int
spawn( const char *executable_path, char *const args[], char *const env[],
			int *arg_stdin, int *arg_stdout, int *arg_stderr,
			pid_t *child_pid )
{
	int function_return_status = 0; /* generic return variable used through out function */
	int	my_stdin[2]; /* 0 */
	int my_stdout[2]; /* 1 */
	int my_stderr[2]; /* 2 */
	
	function_return_status = setup_pipes( &my_stdin, &my_stdout, &my_stderr ); 
	if ( function_return_status )
	{
#ifdef DEBUG
		fprintf( stderr, "Trouble getting pipe file descriptors!\n" );
#endif
		return function_return_status;	/* This may leak file descriptors 
										 *because we don't know if any were allocated
										 */
	}
	
	child_pid = fork();
	if ( child_pid == -1 )
	{
#ifdef DEBUG
		fprintf( stderr, "Trouble forking!\n" );
#endif
		return child_pid;
	}
	else if( child_pid == 0 )
	{
		/* Redirect input, output and error to custome pipes */
		function_return_status = child_redirect_pipes( &my_stdin, &my_stdout, &my_stderr );
		if ( function_return_status < 0)
			goto close_pipes_exit;  /* fprintf error handled in child_redirect_pipes */
		
		/* Replace the child fork with a new process */
		if( execve( executable_path, args, env ) == -1 )
		{
			fprintf( stderr,"execve Error!\n" );
			goto close_pipes_exit;
		}
	}
	else if ( child_pid > 0 )
	{
		/* close opposite pipes owned by client */
		if ( my_stdin )
			close( my_stdin[0] );
		if ( my_stdout )
			close( my_stdout[1] );
		if ( my_stderr )
			close( my_stderr[1] );
		
		/* copy and close unnecissary pipes */
		if ( arg_stdin != NULL )
			*arg_stdin = my_stdin[1];
		else if ( arg_stdin == NULL )
			close( my_stdin[1] );
		
		if ( arg_stdout != NULL)
			*arg_stdout = my_stdout[0];
		else if ( arg_stdout == NULL  )
			close( my_stdout[0] );
		
		if ( arg_stderr != NULL )
			*arg_stderr = my_stderr[0];
		else if( arg_stderr == NULL )
			close( my_stderr[0] );
		
		return 0;
	}
	
/*
* Hate using goto statements, but simplest way to dry up code for exception handling.
*/

close_pipes_exit:		/* This may try to close already closed file descriptors, 
						 * but it will just return an error, no harm*/
	if ( my_stdin != NULL)
	{
		close( my_stdin[0] );
		close( my_stdin[1] );
	}
	
	if ( my_stdout != NULL)
	{
		close( my_stdout[0] );
		close( my_stdout[1] );
	}
	
	if ( my_stderr != NULL )
	{
		close( my_stderr[0] );
		close( my_stderr[1] );
	}
	
	fprintf( stderr, "Exiting spawn after cleaning up file descriptors for unknown reason!\n" );
	exit( 1 );
	
}

/*
* Sets up all the necissary file descriptors for the pipes.  Just more readable to separate
* into a function of its own.
*/

static int
setup_pipes( const int *my_stdin, const int *my_stdout, const int *my_stderr )
{
	int function_return_status = 0;
	
	if ( my_stdin != 0 )
	{
		function_return_status = pipe( my_stdin );
		if ( function_return_status )
		{
#ifdef DEBUG
			fprintf( stderr, "Stdin pipe allocation error!\n" );
#endif
			return function_return_status;
		}
	}

	if ( my_stdout != NULL)
	{
		function_return_status = pipe( my_stdout );
		if ( function_return_status )
		{
#ifdef DEBUG
			fprintf( stderr, "Stdout pipe allocation error!\n" );
#endif
			return function_return_status;
		}
	}
	
	if ( my_stderr != NULL )
	{
		function_return_status = pipe( my_stderr );
		if ( function_return_status )
		{
#ifdef DEBUG
			fprintf( stderr, "Stderr pipe allocation error!\n" );
#endif
			return function_return_status;
		}
	}
	
	return 0;
}

/*
* Redirects all stdin, stdout and stderr to our custome pipes.  Internal use only.
*/

static int
child_redirect_pipes( const int *my_stdin, const int *my_stdout, const int *my_stderr )
{
	int function_return_status = 0;
		
	if ( my_stdin != NULL )
	{
		function_return_status = dup2( my_stdin[0], 0 );	/* Replace stdin with the write side of the stdin pipe */
		if ( function_return_status < 0 )
			goto error_occured;
		else close( my_stdin[1] );
	}
		
	if ( my_stdout != NULL)
	{
		function_return_status = 0;
		function_return_status = dup2( my_stdout[1], 1 );	/* Replace stdout with the read side of the stdout pipe */
		if ( function_return_status < 0 )
			goto error_occured;
		else close( my_stdout[0] );
	}
		
	if ( my_stdout != NULL)
	{
		function_return_status = 0;
		function_return_status = dup2( my_stderr[1], 2 );	/* Replace stderr with the read side of the stderr pipe */
		if ( function_return_status < 0 )
			goto error_occured;
		else close( my_stderr[0] );
	}
	
	return 0;

error_occured:
#ifdef DEBUG
	fprintf( stderr, "Could not redirect stdin, stdout or stderr\n" );
#endif	
	return function_return_status;
}

#endif