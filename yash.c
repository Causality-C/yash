#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

// Macros
#define OUTPUT_REDIRECT ">"
#define INPUT_REDIRECT "<"
#define ERR_REDIRECT "2>"
#define ONE_SPACE " "
#define PIPE "|"
#define PROMPT "swagshell> " // TODO: CHANGE
#define ZERO 0
#define DEBUG 0
#define MAX_JOBS 20

typedef struct process{
  char ** argv;
  char * input_redir_file;
  char * output_redir_file;
  char * err_redir_file;
  char * token;
  int iarg;
  int fp;
} Process;
/**
 * Parse the command until the nearest pipe
 * Return 0 if pipe not encountered
 * 1 if the pipe has been encountered
 * TODO: -1 if there has been a parsing error
 * */
int parse_command(Process * process, char ** argv){
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
      argv[process->iarg++] = NULL; // NULL Terminate the Array 
      return 1; // Pipe detected: GTFO
    }
    // CUSTOM COMMAND FREE PROCESS 
    else if(strcmp(token,"fp") == ZERO){      
      process->fp = 1;
      argv[process->iarg] = malloc(sizeof(char) *strlen(token) + 1);
      strcpy(argv[process->iarg++],token);
    }
    else{
      argv[process->iarg] = malloc(sizeof(char) *strlen(token) + 1);
      strcpy(argv[process->iarg++],token);
    }
    token = strtok(NULL, ONE_SPACE);
  }	
  // Null Terminate arg array
  argv[process->iarg] = NULL; 
  return 0;
}

void check_redirects(Process * process){
  if(process->input_redir_file != NULL){
    int redirect_fd = open(process->input_redir_file, O_RDONLY);
    dup2(redirect_fd, STDIN_FILENO);
    close(redirect_fd);
  }
  if(process->output_redir_file != NULL){
    int redirect_fd = open(process->output_redir_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
    dup2(redirect_fd, STDOUT_FILENO);
    close(redirect_fd);
  }
  if(process->err_redir_file != NULL){
    int redirect_fd = open(process->err_redir_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
    dup2(redirect_fd, STDERR_FILENO);
  }
}

int execute_command(Process * process, char** argv){
  // Create new process, execute command
  pid_t pid = getpid();
  pid_t forked = fork();
  if(forked == ZERO){
    char * cmd = argv[0];
    // printf("SETTING PID OF CHILD: %d %d\n",getpid(), pid );
    // if(setpgid(getpid(),pid) == -1){
    //   printf("ERROR FOUND: %d\n", errno);
    // }
    // printf("Updated GPID of CHILD: %d \n", getpgid(pid));

    // Child Process must be able to terminate
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    
    if(setpgid(0,0) == -1){
      printf("Error occured when created proccess group: %d \n", errno );
    }

    // TODO: SET PGID as foreground process group
    // tcsetpgrp(0,getpid());
    // Sets file descriptors for redirects
    check_redirects(process); 
    if(process->fp){

    }
    else{
      execvp(cmd, argv);
    }

    // Writes invalid cmd to stderr
    if(process->err_redir_file != NULL){
      perror(cmd);
    }
    exit(1); // Exit out if execvp failed
  }else{
    if(DEBUG){
      printf("Parent PID: %d\n", getpid());
      printf("Parent GPID: %d\n", getpgid(getpid()));
      printf("%d %d \n",forked, pid); 
    }
   // setpgid(forked, pid);
  }
  return forked;
}

int execute_command_with_pipe(Process * l_process, Process * r_process, char ** largv, char ** rargv){
  pid_t forked = fork();
  if(forked == ZERO){
    // Commands can be interrupted
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    char * l_cmd = largv[0];
    char * r_cmd = rargv[0];

    int fd[2];
    if(pipe(fd) == -1){
      exit(1);
    }

    // Left Side
    int p1 = fork();
    if(p1 == 0){
      dup2(fd[1], STDOUT_FILENO);
      close(fd[0]);
      close(fd[1]);
      check_redirects(l_process);
      execvp(l_cmd, largv);
    }

    // Right Side
    int p2 = fork();
    if(p2 == 0){
      dup2(fd[0], STDIN_FILENO);
      close(fd[0]);
      close(fd[1]);
      check_redirects(r_process);
      execvp(r_cmd, rargv);
    }

    // Close Pipe and Wait Until Child Processes Are Finished
    close(fd[0]);
    close(fd[1]);
    waitpid(p1,NULL, 0);
    waitpid(p2,NULL, 0 );
    
    // Support Error Redirection
    exit(1); // Exit out if execvp failed
  }
  return forked;
}

int main(){	
	// Create Child Process
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  
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
			char * line_dup = strdup(line);
			char * token = strtok(line_dup, ONE_SPACE);
      char * line_dup2;
      char * token2;

			// Command Parsing
			char * argv[10];
      char * rargv[10];

			// Todo: modify exit condition, free unused blocks
			if(strcmp(line, "exit") == ZERO){
				free(line);
				free(line_dup);
				exit(1);
			}

      // Redirection, there can only be one of each => TODO: CHECK 
      Process process = {argv,NULL,NULL,NULL,token,0,0};
      Process right_process;
      int pipe_exists = parse_command(&process,process.argv);

      if(pipe_exists){
        // Create a new argument array
        line_dup2 = strdup(line);
        token2 = strtok(line_dup2, PIPE);
        token2 = strtok(NULL,ONE_SPACE);
        right_process = (Process) {rargv,NULL,NULL,NULL,token2,0,0}; 
        parse_command(&right_process,right_process.argv); 
      }
      
      // PID of child allows you keep track of status later
      int pid = 0;
      if(pipe_exists){
        pid = execute_command_with_pipe(&process, &right_process, process.argv, right_process.argv);
      }
      else{
        pid = execute_command(&process,process.argv);
      }

      // Process Status
      int stat;
      waitpid(pid,&stat, WUNTRACED); // TODO: Check if WUNTRACED is sufficient
      if(DEBUG){
        printf("GPID of CHILD: %d\n", getpgid(pid));
        printf("%d \n", WIFSTOPPED(stat));
        printf("PID of Child: %d\n", pid);
      }
			// Free  
			free(line);
			free(line_dup);
      for(int j = 0; j< process.iarg; j++){
        free(argv[j]);
      }
      if(pipe_exists){
        for(int j = 0; j< right_process.iarg; j++){
          free(rargv[j]);
        } 
        free(line_dup2);
      }
      // printf("PID of CHILD: %d\n", pid);
      // waitpid(pid,&stat, WUNTRACED);
      // printf("%d \n", WIFSTOPPED(stat));
      // printf("Exit Status: %d\n", WIFSTOPPED(stat));
		}
	}
  // int stat;
  // waitpid(cPid, &stat,WUNTRACED);
	wait(NULL);
	return 0;
}
