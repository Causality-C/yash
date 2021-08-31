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
#define ONE_SPACE " "
#define ZERO 0
#define PROMPT "swagshell> " // TODO: CHANGE
#define DEBUG 0


typedef struct process{
  char ** argv;
  char * input_redir_file;
  char * output_redir_file;
  char * err_redir_file;
  char * token;
  int iarg;
} Process;

// TODO: return statement can determine error
void parse_command(Process * process, char ** argv){
  char * token = process->token;
  // Tokenize Strings
  while(token != NULL){
    // STDOUT 
    if(strcmp(token, OUTPUT_REDIRECT) == ZERO){          
      token = strtok(NULL, ONE_SPACE);
      if(token != NULL){
        process->output_redir_file = token;
      }
    }
    // STDIN 
    else if(strcmp(token, INPUT_REDIRECT) == ZERO){
      token = strtok(NULL, ONE_SPACE);
      if(token != NULL){
        process->input_redir_file = token;
      }
    }
    // STDERR 
    else if(strcmp(token, ERR_REDIRECT) == ZERO){
      token = strtok(NULL, ONE_SPACE);
      if(token != NULL){
        process->err_redir_file = token;
      }
    }
    else{
      argv[process->iarg] = malloc(sizeof(char) *strlen(token) + 1);
      strcpy(argv[process->iarg++],token);
    }
    token = strtok(NULL, ONE_SPACE);
  }	
  // Null Terminate arg array
  argv[process->iarg] = NULL; 
}
void execute_command(Process * c_redir, char** argv){
  // Create new process, execute command
  pid_t forked = fork();
  if(forked == ZERO){
    char * cmd = argv[0];
    // Check File Redirects
    if(c_redir->input_redir_file != NULL){
      int redirect_fd = open(c_redir->input_redir_file, O_RDONLY);
      dup2(redirect_fd, STDIN_FILENO);
      close(redirect_fd);
    }
    if(c_redir->output_redir_file != NULL){
      int redirect_fd = open(c_redir->output_redir_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
      dup2(redirect_fd, STDOUT_FILENO);
      close(redirect_fd);
    }
    if(c_redir->err_redir_file != NULL){
      int redirect_fd = open(c_redir->err_redir_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
      dup2(redirect_fd, STDERR_FILENO);
    }

    execvp(cmd, argv);

    // Writes invalid cmd to stderr
    perror(cmd);
    exit(1); // Exit out if execvp failed
  }
}

int main(){	
	// Create Child Process
	pid_t cPid = fork();

	// The fork failed
	if(cPid == -1){
		exit(1);
	}
  // Initial Parent
	else if(cPid > ZERO){}
	else{
		while(1){
			char * line = readline(PROMPT);
			char * dup = strdup(line);
			char * token = strtok(dup, ONE_SPACE);

			// Command Parsing
			char * argv[10];

			// Todo: modify exit condition, free unused blocks
			if(strcmp(line, "exit") == ZERO){
				free(line);
				free(dup);
				exit(1);
			}

      // Redirection, there can only be one of each => TODO: CHECK 
      Process process = {argv,NULL,NULL,NULL,token,0};
      parse_command(&process,process.argv);
      execute_command(&process,process.argv);

			// Free  
			free(line);
			free(dup);
      for(int j = 0; j< process.iarg; j++){
        free(argv[j]);
      }
			wait(NULL);
		}
	}

	wait(NULL);
	return 0;
}
