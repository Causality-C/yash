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
#define PIPE "|"
#define PROMPT "swagshell> " // TODO: CHANGE
#define ZERO 0
#define DEBUG 0


typedef struct process{
  char ** argv;
  char * input_redir_file;
  char * output_redir_file;
  char * err_redir_file;
  char * token;
  int iarg;
  int pipe;
} Process;

// TODO: return statement can determine error
void parse_command(Process * process, char ** argv){
  char * token = process->token;
  // Tokenize Strings
  while(token != NULL){

    if(DEBUG){
      printf("%s %d\n",token, process->iarg);
    }
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
    // Pipe
    else if(strcmp(token, PIPE) == ZERO){
      process->pipe = process->iarg;
      argv[process->iarg++] = NULL;
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
    
    // Pipe -> TODO: INCLUDE REDIRECTION FOR PIPES
    if(c_redir->pipe != -1){
      int fd[2];
      if(pipe(fd) == -1){
        exit(1);
      }

      char * rcmd = argv[c_redir->pipe + 1];
      char ** rargv = &argv[c_redir->pipe + 1];

      // Left Side
      int p1 = fork();
      if(p1 == 0){
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        execvp(cmd,argv);
      }

      // Right Side
      int p2 = fork();
      if(p2 == 0){
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        execvp(rcmd, rargv);
      }

      // Close Pipe and Wait Until Child Processes Are Finished
      close(fd[0]);
      close(fd[1]);
      waitpid(p1,NULL,0);
      waitpid(p2,NULL,0);

      if(DEBUG){
        for(int i = 0; i< c_redir->pipe; i++){
          printf("%s ", c_redir->argv[i]);
        }
        printf("\n");
        for(int i = c_redir->pipe + 1; i< c_redir->iarg; i++){
          printf("%s ", c_redir->argv[i]);
        } 
        printf("\n");
      }
    }
    else{
      execvp(cmd, argv);
    }

    // Writes invalid cmd to stderr
    if(c_redir->err_redir_file != NULL){
      perror(cmd);
    }
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
      Process process = {argv,NULL,NULL,NULL,token,0,-1};
      parse_command(&process,process.argv);
      if(DEBUG){
        printf("PIPE: %d\n", process.pipe);
      }
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
