#include "exec.h"
#include <unistd.h>
#include "defs.h"
#include "printstatus.h"

extern int status;

static void
create_pipe(int fds[])
{
	int r = pipe(fds);
	if (r < 0) {
		perror("Pipe error");
		_exit(-1);
	}
}

static int
create_fork()
{
	int i = fork();
	if (i < 0) {
		perror("Fork error");
		_exit(-1);
	}
	return i;
}

// sets "key" with the key part of "arg"
// and null-terminates it
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  key = "KEY"
//
static void
get_environ_key(char *arg, char *key)
{
	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
// "idx" should be the index in "arg" where "=" char
// resides
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  value = "value"
//
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
//
// Hints:
// - use 'block_contains()' to
// 	get the index where the '=' is
// - 'get_environ_*()' can be useful here
static void
set_environ_vars(char **eargv, int eargc)
{
	char key[BUFLEN];
	char value[BUFLEN];

	for (int i = 0; i < eargc; i++) {
		get_environ_key(eargv[i], key);

		int idx = block_contains(eargv[i], '=');
		get_environ_value(eargv[i], value, idx);

		if ((setenv(key, value, 1)) < 0) {
			perror(key);
			_exit(-1);
		}
	}
}

static int
dup_file(int old_fd, int new_fd)
{
	int fd;
	if ((fd = dup2(old_fd, new_fd)) < 0) {
		perror("Dup error");
		_exit(-1);
	}
	return fd;
}


// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
//
// Find out what permissions it needs.
// Does it have to be closed after the execve(2) call?
//
// Hints:
// - if O_CREAT is used, add S_IWUSR and S_IRUSR
// 	to make it a readable normal file
static int
open_redir_fd(char *file, int flags)
{
	int fd;

	if (flags >= (O_CREAT | O_WRONLY | O_CLOEXEC)) {
		if ((fd = open(file, flags, S_IRUSR | S_IWUSR)) < 0) {
			perror(file);
			_exit(-1);
		}

	} else {
		if ((fd = open(file, flags, S_IRUSR)) < 0) {
			perror(file);
			_exit(-1);
		}
	}
	return fd;
}


// executes a command - does not return
//
// Hint:
// - check how the 'cmd' structs are defined
// 	in types.h
// - casting could be a good option
void
exec_cmd(struct cmd *cmd)
{
	// To be used in the different cases
	struct execcmd *e;
	struct backcmd *b;
	struct execcmd *r;
	struct pipecmd *p;

	switch (cmd->type) {
	case EXEC:
		// spawns a command
		e = (struct execcmd *) cmd;

		if (e->eargc > 0)
			set_environ_vars(e->eargv, e->eargc);

		if (execvp(e->argv[0], e->argv) < 0) {
			perror(e->argv[0]);
			_exit(-1);
		}

		break;

	case BACK: {
		// runs a command in background
		b = (struct backcmd *) cmd;

		exec_cmd(b->c);  // c->type is EXEC
		break;
	}

	case REDIR: {
		// changes the input/output/stderr flow

		int fd;
		r = (struct execcmd *) cmd;

		if (strlen(r->in_file) > 0) {
			fd = open_redir_fd(r->in_file, O_RDONLY | O_CLOEXEC);
			dup_file(fd, STDIN_FILENO);
		}

		if (strlen(r->out_file) > 0) {
			fd = open_redir_fd(r->out_file,
			                   O_CREAT | O_TRUNC | O_WRONLY |
			                           O_CLOEXEC);
			dup_file(fd, STDOUT_FILENO);
		}

		if (strlen(r->err_file) > 0) {
			if (r->err_file[0] == '&') {
				dup_file(STDOUT_FILENO,
				         STDERR_FILENO);  // redir flow from stderr
				                          // to stdout (which points to r->out_file)

			} else {
				fd = open_redir_fd(r->err_file,
				                   O_CREAT | O_TRUNC |
				                           O_WRONLY | O_CLOEXEC);
				dup_file(fd, STDERR_FILENO);
			}
		}

		r->type = EXEC;
		exec_cmd((struct cmd *) r);

		break;
	}

	case PIPE: {
		p = (struct pipecmd *) cmd;
		int fds[2];

		create_pipe(fds);

		// left fork
		int left_fork = create_fork();

		if (left_fork == 0) {
			close(fds[READ]);
			dup_file(fds[WRITE],
			         STDOUT_FILENO);  // redir stdout flow of left cmd to WRITE side of pipe
			close(fds[WRITE]);
			exec_cmd(p->leftcmd);
		}

		// right fork
		int right_fork = create_fork();

		if (right_fork == 0) {
			close(fds[WRITE]);
			dup_file(fds[READ],
			         STDIN_FILENO);  // redir stdin flow of right cmd to left cmd stdout
			close(fds[READ]);
			exec_cmd(p->rightcmd);
		}

		if (left_fork > 0) {
			close(fds[WRITE]);
			waitpid(p->leftcmd->pid, &status, 0);
			print_status_info(p->leftcmd);
		}

		if (right_fork > 0) {
			close(fds[READ]);
			waitpid(p->rightcmd->pid, &status, 0);
			print_status_info(p->rightcmd);

			_exit(0);
		}

		free_command(parsed_pipe);
		break;
	}
	}
}
