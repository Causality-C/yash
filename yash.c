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
#define PLUS "+"
#define MINUS "-"
#define PROMPT "swagshell> " // TODO: CHANGE
#define ZERO 0
#define DEBUG 0
#define MAX_JOBS 20

// Custom Commands to implement
enum custom_commands{NONE,FG,BG,JOBS};
enum job_status{RUNNING,STOPPED,DONE,TERMINATED};
enum parse_returns{NONE_FOUND,PIPE_FOUND,AMPERSAND_FOUND};

typedef struct process{
  char ** argv;
  char * input_redir_file;
  char * output_redir_file;
  char * err_redir_file;
  char * token;
  int iarg;
  int custom_command;
  int background;
} Process;

typedef struct job{
  char * command; 
  int id;
  Process * lprocess;
  Process * rprocess;
  pid_t pgrp;
  int status;
} Job;

// Define all Functions: TODO
int parse_command(Process * process, char ** argv);
void check_redirects(Process * process);
void handle_done_jobs();
int execute_custom_command(Job * job);
int execute_command(Job * job, char ** argv);
int execute_command_with_pipe(Job * job);
static void child_sig_handler();
void print_job_table();
void free_process(Process * p, char * line);

// Store Jobs/Process Groups in a stack (LIFO), Job Table Index
Job * job_table [MAX_JOBS];
int jtindx = 0;

/**
 * Parse the command until the nearest pipe or ampersand
 * Return 0 if pipe not encountered
 * 1 if the pipe has been encountered
 * 2 if an amperseand has been encountered
 * TODO: -1 if there has been a parsing error
 * */
int parse_command(Process * process, char ** argv){
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
    // Pipe
    else if(strcmp(token, PIPE) == ZERO){
      argv[process->iarg++] = NULL; // NULL Terminate the Array 
      return PIPE_FOUND; // Pipe detected: GTFO
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
    // Ampersand -> send job to background
    else if(strcmp(token,"&") == ZERO){
      argv[process->iarg++] = NULL;
      return AMPERSAND_FOUND;
    }
    else{
      argv[process->iarg] = malloc(sizeof(char) *strlen(token) + 1);
      strcpy(argv[process->iarg++],token);
    }
    token = strtok(NULL, ONE_SPACE);
  }	
  // Null Terminate arg array
  argv[process->iarg] = NULL; 
  return NONE_FOUND;
}

/**
 * Sets the file descriptors for file redirection on a single process
 * Supports ... 
 * - Input Redirection
 * - Output Redirection
 * - Error Redirection
 * */
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


/**
 * Prints out all jobs done before the input of the command
 * Frees memory of a consecutive sequence of DONE or TERMINATED indices ending at the current jtindx
 * If sequence does not exist, they will not be freed 
 * */
void handle_done_jobs(){
  // Index to start freeing memory from,
  int free_start_index = -1;
  for(int i = 0; i < jtindx; i++){
    Job * job = job_table[i];
    char * identifier = (i == jtindx-1) ? PLUS: MINUS;
    int status = job->status; 

    // Set the last_done_job index
    if(status == DONE){
      free_start_index = (free_start_index == -1) ? i: free_start_index;
      printf("[%d] %s Done                  %s\n", job->id, identifier,job->command);
      job->status = TERMINATED;
    }
    else{
      free_start_index = (status == TERMINATED) ? ((free_start_index == -1) ? i: free_start_index) : -1;
    }
  }
  // Free Memory
  if(free_start_index != -1 && free_start_index < jtindx){
    for(int i = free_start_index; i< jtindx; i++){
      free(job_table[i]->command);
      free(job_table[i]);
    }
    jtindx = free_start_index;
  }
}

/**
 * Executes a custom command (jobs, bg, fg) 
 * Returns ...
 * - Group ID if the PGRP for a foreground process
 * - Negative Number else (invalid PID)
 * */
int execute_custom_command(Job * job){
  Process * process = job->lprocess;

  // Super hacky way of doing fg, bg, and jobs
  if(process->custom_command && jtindx != ZERO){
    int custom_command = process->custom_command;
    // Only list stuff out on the table if they exist
    int next_bg_job;
    if(jtindx > ZERO){
      switch(custom_command){
        case FG:
          jtindx--;
          kill(-job_table[jtindx]->pgrp,SIGCONT);
          job_table[jtindx]->status = RUNNING;
          tcsetpgrp(ZERO, job_table[jtindx]->pgrp);
          return job_table[jtindx]->pgrp;
          break;
        case BG: 
          // Prevent Segfault, find the nearest process
          next_bg_job = jtindx-1;
          while(next_bg_job >= ZERO && job_table[next_bg_job]->status == RUNNING){
            next_bg_job--;
          }
          if(next_bg_job != -1){
            kill(-job_table[next_bg_job]->pgrp,SIGCONT);
            job_table[next_bg_job]->status = RUNNING; 
          }
         break;
        case JOBS:
          print_job_table();
          return -2;
          break;
        default:
          break;
      }
    }  
  }
  return -1;
}

int execute_command(Job * job, char** argv){
  // Create new process, execute command
  Process * process = job->lprocess;
  
  pid_t pid = getpid();
 
  pid_t forked = fork();

  if(forked == ZERO){
    // Empty Input Edge Case
    char * cmd = process->iarg == 0 ? "": argv[0];

    // Child Process must be able to terminate, cannot inherit dispositions => signal mask 
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTOU, SIG_IGN);

    // Creates a new process group
    if(setpgid(0,0) == -1){
      printf("Error occured when created proccess group: %d \n", errno );
    }
    
    // Prevent Race Conditions, set PID as foreground process group
    if(!process->background){
      tcsetpgrp(0,getpid());
    }

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

int execute_command_with_pipe(Job* job){
  // Variables Needed
  Process * l_process = job->lprocess; 
  Process * r_process = job->rprocess;
  char ** largv = l_process->argv;
  char ** rargv = r_process->argv;

  pid_t forked = fork();
  if(forked == ZERO){
    // Commands can be interrupted
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    char * l_cmd = l_process->iarg == 0? "": largv[0];
    char * r_cmd = r_process->iarg == 0? "": rargv[0];

    int fd[2];
    if(pipe(fd) == -1){
      exit(1);
    }
    // Creates a new process group
    if(setpgid(0,0) == -1){
      printf("Error occured when created proccess group: %d \n", errno );
    }
    // Prevent Race Conditions, set PID as foreground process group
    if(!l_process->background){
      tcsetpgrp(0,getpid());
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

static void child_sig_handler(){
  pid_t pid;
  int status;
  pid = waitpid(-1, &status, WNOHANG); // Non - blocking

  if(pid > 0){
    //Go over table and mark completion
    for(int i = 0; i< jtindx; i++){
      Job *j = job_table[i];
      if(j->pgrp == pid){
        j->status = DONE;
      }
    }
  }

}

void print_job_table(){
  for(int i = 0; i< jtindx; i++){
    Job * job = job_table[i];
    char * identifier = (i == jtindx - 1) ? PLUS: MINUS;
    int status = job->status;
    switch(status){
      case RUNNING:
        printf("[%d] %s Running              %s\n", job->id, identifier,job->command);
        break;
      case STOPPED:
        printf("[%d] %s Stopped              %s\n", job->id, identifier,job->command);
        break; 
      default:
        break;
    }
  }
}

void free_process(Process * p, char * line){
  free(line);
  for(int j = 0; j< p->iarg; j++){
    free(p->argv[j]);
  }
}

int main(){	
	// Create Child Process
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGCHLD, child_sig_handler);
  
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
    Job * curr_job = (Job*) malloc(sizeof(Job));
    curr_job->command = line;


    Process process = {argv,NULL,NULL,NULL,token,0,0,0};
    Process right_process;

    // Parse Status
    int parse_status = parse_command(&process,process.argv);
    curr_job->lprocess = &process;

    int pipe_exists = parse_status == PIPE_FOUND;
    int bg_exists = parse_status == AMPERSAND_FOUND;

    if(pipe_exists){
      // Create a new argument array
      line_dup2 = strdup(line);
      token2 = strtok(line_dup2, PIPE);
      token2 = strtok(NULL,ONE_SPACE);
      right_process = (Process) {rargv,NULL,NULL,NULL,token2,0,0,0}; 

      parse_status = parse_command(&right_process,right_process.argv); 
      curr_job->rprocess = &right_process;
      bg_exists = parse_status == AMPERSAND_FOUND;
    }

    if(bg_exists){
      process.background = 1;
    }

    // PID of child allows you keep track of process group created
    int pid = 0;

    // Must be called before fork : prints out all done jobs
    handle_done_jobs();

    if(pipe_exists){
      pid = execute_command_with_pipe(curr_job);
    }
    // Custom Commands for : BG, JOBS, FG
    else if(process.custom_command){
      pid = execute_custom_command(curr_job);
    }
    else{
      pid = execute_command(curr_job, process.argv);
    }

    // Set Job Values
    curr_job->pgrp = pid;
    curr_job->status = RUNNING;

    // Process Status
    int stat;
    int background_status = (pipe_exists) ? !(process.background || right_process.background): !process.background;
    // If process runs in the background, we don't want to wait, 
    if (background_status && pid >= 1){

      waitpid(pid, &stat, WUNTRACED); // Blocking wait

      // If interrupted, then add process group/job onto stack
      if(WIFSTOPPED(stat) && jtindx < MAX_JOBS){
        // FG Process must be checked 
        if(strcmp(line,"fg") == ZERO){
          curr_job->command = job_table[jtindx]->command;
        }
        curr_job->id = jtindx + 1;
        curr_job->status = STOPPED;
        job_table[jtindx++] = curr_job;
      }

      // Deallocate the Job and free everything if not interrupted
      else{
        free(curr_job->command);
        free(curr_job);
      }

      stat = 0; // Apparently, stat keeps its value so we have to reset it

      // Return Control to the terminal 
      tcsetpgrp(0,getpgid(0));
    }
    else if (process.background){
      curr_job->id = jtindx + 1;
      job_table[jtindx++] = curr_job;
    }
    // Custom Command
    else if(pid < 1){
      free(curr_job->command);
      free(curr_job);
    }

    // Free  
    free_process(&process,line_dup);
    if(pipe_exists){
      free_process(&right_process,line_dup2);
    }
  }
}
