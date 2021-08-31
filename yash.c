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
#define DEBUG 1

enum redirection_enum{INPUT,OUTPUT,ERR};

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

      // Redirection 
      int file_redir[3] = {-1,-1,-1};
      int file_redir_index = 0;
      int jump = 0;
      char * filename;

			// Tokenize Strings
			while(token != NULL){
        // STDOUT REDIRECTION
        if(strcmp(token, OUTPUT_REDIRECT) == 0){

          jump = 1;

          file_redir[file_redir_index] = OUTPUT;
          file_redir_index++;

          token = strtok(NULL," ");
          if(token != NULL){
            filename = token;
          }
        }
        // STDIN REDIRECTION
        else if(strcmp(token, INPUT_REDIRECT) == 0){
          file_redir[file_redir_index] = INPUT;
          file_redir_index++;
        
        }
        // STDERR REDIRECTION
        else if(strcmp(token, ERR_REDIRECT) == 0){
          file_redir[file_redir_index] = ERR;
          file_redir_index++;
        }
        else{
          cmd[i] = malloc(sizeof(char) *strlen(token) + 1);
          strcpy(cmd[i++],token);
        }
        token = strtok(NULL," ");
			}	
      cmd[i] = NULL;
    
      if(DEBUG){
        for(int j = 0; j< i; j++){
          printf("%s ",cmd[j]);
        }
        printf("\n"); 
      }
      if(DEBUG){
        for(int j = 0; j<3; j++){
          printf("%d ", file_redir[j]);
        }
        printf("\n");
      }

			// Create new process, execute command
			pid_t forked = fork();
			if(forked == 0){
        if(jump){
          // Open redirected source in child
          close(STDOUT_FILENO);
          open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
        }
				execvp(cmd[0], cmd);
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
