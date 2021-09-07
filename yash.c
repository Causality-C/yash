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

// Global Variables
int process_groups [MAX_JOBS];
int pgindx = 0;

typedef struct process{
  char ** argv;
  char * input_redir_file;
  char * output_redir_file;
  char * err_redir_file;
  char * token;
  int iarg;
  int custom_command;
} Process;

// Custom Commands to implement
enum commands{NONE, FG, BG,JOBS };

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
    // FG 
    else if(strcmp(token,"fg") == ZERO){      
      process->custom_command  = FG;
      argv[process->iarg] = malloc(sizeof(char) *strlen(token) + 1);
      strcpy(argv[process->iarg++],token);
    } 
    // BG
    else if(strcmp(token,"bg") == ZERO){      
      process->custom_command  = BG;
      argv[process->iarg] = malloc(sizeof(char) *strlen(token) + 1);
      strcpy(argv[process->iarg++],token);
    } 
    // JOBS
    else if(strcmp(token,"jobs") == ZERO){      
      process->custom_command  = JOBS;
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

int execute_custom_command(Process *process){
  // Super hacky way of doing the fg command
  if(process->custom_command && pgindx != 0){
    int custom_command = process->custom_command;
    // Only list stuff out on the table if they exist
    if(pgindx > 0){
      switch(custom_command){
        case FG:
          pgindx--;
          kill(-process_groups[pgindx], SIGCONT);
          tcsetpgrp(0, process_groups[pgindx]);
          return process_groups[pgindx];
          break;
        case BG:
          pgindx--;
          kill(-process_groups[pgindx], SIGCONT);
          break;
        case JOBS:
          break;
        default:
          break;
      }
    }  
  }
  return -1;
}
int execute_command(Process * process, char** argv){
  // Create new process, execute command
  pid_t pid = getpid();
  pid_t forked = fork();
  if(forked == ZERO){
    char * cmd = argv[0];

    // Child Process must be able to terminate, cannot inherit dispositions => signal mask 
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTOU, SIG_IGN);

    // Creates a new process group
    if(setpgid(0,0) == -1){
      printf("Error occured when created proccess group: %d \n", errno );
    }

    // Prevent Race Conditions, set PID as foreground process group
    tcsetpgrp(0,getpid());

    // Sets file descriptors for redirects
    check_redirects(process); 
    execvp(cmd,argv);

    // Writes invalid cmd to stderr
    if(process->err_redir_file != NULL){
      perror(cmd);
    }
    exit(1); // Exit out if execvp failed
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
    // Creates a new process group
    if(setpgid(0,0) == -1){
      printf("Error occured when created proccess group: %d \n", errno );
    }
    tcsetpgrp(0,getpid());

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
  signal(SIGTTOU, SIG_IGN);
  
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
    
    // PID of child allows you keep track of process group created
    int pid = 0;
    if(pipe_exists){
      pid = execute_command_with_pipe(&process, &right_process, process.argv, right_process.argv);
    }
    // Custom Commands for : BG, JOBS, FG
    else if(process.custom_command){
      pid = execute_custom_command(&process);
    }
    else{
      pid = execute_command(&process,process.argv);
    }

    // Process Status
    int stat;
    // If PID is -1, we're not waiting for anything 
    if (pid != -1){
      waitpid(pid,&stat, WUNTRACED); // TODO: Check if WUNTRACED is sufficient
      // If interrupted, then add process group/job onto stack
      if(WIFSTOPPED(stat) && pgindx < MAX_JOBS){
        process_groups[pgindx++] = pid;
      }
      tcsetpgrp(0,getpgid(0));
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
  }
}
