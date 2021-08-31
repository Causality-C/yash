#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

// Macros
#define OUTPUT_REDIRECT ">"
#define INPUT_REDIRECT "<"
#define ERR_REDIRECT "2>"
#define PROMPT "swagshell> " // TODO: CHANGE
#define DEBUG 0

int main(){	
	// Create Child Process
	pid_t cPid = fork();

	// The fork failed
	if(cPid == -1){
		exit(1);
	}
  // Initial Parent
	else if(cPid > 0){}
	else{
		while(1){
			char * line = readline(PROMPT);
			char * dup = strdup(line);
			char * token = strtok(dup," ");

			// Command Parsing
			char * cmd[10];
			int i = 0;

			// Todo: modify exit condition, free unused blocks
			if(strcmp(line, "exit") == 0){
				free(line);
				free(dup);
				exit(1);
			}

      // Redirection, there can only be one of each => TODO: CHECK 
      char * file_redir_names[3];
      int jump = 0;
      char * filename;
      int input_redir = 0;
      int output_redir = 0;
      int err_redir = 0;

			// Tokenize Strings
			while(token != NULL){
        // STDOUT REDIRECTION
        if(strcmp(token, OUTPUT_REDIRECT) == 0){          
          output_redir = 1;
          token = strtok(NULL," ");
          if(token != NULL){
            file_redir_names[STDOUT_FILENO] = token;
          }
        }
        // STDIN REDIRECTION
        else if(strcmp(token, INPUT_REDIRECT) == 0){
          input_redir = 1; 
          token = strtok(NULL, " ");
          if(token != NULL){
            file_redir_names[STDIN_FILENO] = token;
          }
        }
        // STDERR REDIRECTION
        else if(strcmp(token, ERR_REDIRECT) == 0){
          err_redir = 1;
          token = strtok(NULL, " ");
          if(token != NULL){
            file_redir_names[STDERR_FILENO] = token;
          }
        }
        else{
          cmd[i] = malloc(sizeof(char) *strlen(token) + 1);
          strcpy(cmd[i++],token);
        }
        token = strtok(NULL," ");
			}	
      // Null Terminate Arg array
      cmd[i] = NULL;
    
      if(DEBUG){
        for(int j = 0; j< i; j++){
          printf("%s ",cmd[j]);
        }
        printf("\n"); 
      }
      if(DEBUG){
        printf("%d\n", STDOUT_FILENO);
      }

			// Create new process, execute command
			pid_t forked = fork();
			if(forked == 0){
        // Check File Redirects
        if(input_redir){
          int redirect_fd = open(file_redir_names[STDIN_FILENO], O_RDONLY);
          dup2(redirect_fd, STDIN_FILENO);
          close(redirect_fd);
        }
        if(output_redir){
          int redirect_fd = open(file_redir_names[STDOUT_FILENO], O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
          dup2(redirect_fd, STDOUT_FILENO);
          close(redirect_fd);
        }
        if(err_redir){
          int redirect_fd = open(file_redir_names[STDERR_FILENO], O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
          dup2(redirect_fd, STDERR_FILENO);
        }
				execvp(cmd[0], cmd);
        
        // Writes invalid cmd to stderr
        perror(cmd[0]);
			}

			// Free  
			free(line);
			free(dup);
      for(int j = 0; j< i; j++){
        free(cmd[j]);
      }
			wait(NULL);
		}
	}

	wait(NULL);
	return 0;
}
