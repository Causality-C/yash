#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(){	
	// Create Child Process
	pid_t cPid = fork();

	// The fork failed
	if(cPid == -1){
		exit(1);
	}
	else if(cPid > 0){}
	else{
		while(1){
			char * line = readline("swagshell> ");
			char * dup = strdup(line);
			char * token = strtok(dup," ");

			// Command Parsing
			char * cmd[10];
			int i = 0;
			cmd[i++] = token;

			// Todo: modify exit condition, free unused blocks
			if(strcmp(line, "exit") == 0){
				free(line);
				free(dup);
				exit(1);
			}

			// Tokenize Strings
			while(token != NULL){
				token = strtok(NULL," ");
				cmd[i++] = token;
			}

			// NULL TERMINATED ARRAY FOR ARGS
			cmd[i] = NULL;
			
			// Create new process, execute command
			pid_t forked = fork();
			if(forked == 0){
				execvp(cmd[0], cmd);
			}

			// Free  
			free(line);
			free(dup);
			wait(NULL);
		}
	}

	wait(NULL);
	return 0;
}
