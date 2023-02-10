#include "builtin.h"
#include "utils.h"
extern int status;

//#include <string.h>

// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd)
{
	return ((strcmp(cmd, "exit")) == 0);
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd)
{
	if (cmd[0] != 'c' || cmd[1] != 'd')
		return false;

	char *new_dir = split_line(cmd, ' ');

	if (strlen(new_dir) == 0)
		new_dir = getenv("HOME");

	if (chdir(new_dir) < 0) {
		perror(new_dir);
		status = 1;

	} else {
		char cwd[BUFLEN];
		snprintf(prompt, sizeof prompt, "(%s)", getcwd(cwd, BUFLEN));
	}

	status = 0;
	return true;
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd)
{
	if (strcmp(cmd, "pwd") == 0) {
		char cwd[BUFLEN];
		printf_debug("%s\n", getcwd(cwd, BUFLEN));

		status = 0;
		return true;
	}
	return false;
}
