#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "linked_list.h"

Node* head = NULL;


/*
* Start a new process in the background
* running the command specified by the user.
*/
void func_BG(char **cmd){
    if(cmd[1] == NULL) {
      fprintf(stderr, "Error: Please enter a command to run.\n");
      return;
    }

    // Create the child process
    pid_t p_val = fork();

    if(p_val < 0) {
      // Error has occured
      fprintf(stderr, "Error: Could not run %s. Could not create new process.\n", cmd[1]);
      return;
    } else if(p_val == 0) {
      // Only run in the child process
      // Replace code with the new command
      execvp(cmd[1], &(cmd[1]));
      printf("\n");
      perror("execvp");
      exit(-1);
    } else {
      // Add child to linked list
      head = add_newNode(head, p_val, cmd[1]);
      
      // Check the status of the child we just created
      int p_status;
      int pid = waitpid(p_val, &p_status, WNOHANG);
      if(pid < 0) perror("waitpid");
      if(WIFEXITED(p_status)) {
        head = deleteNode(head, pid);                
      }
    }
}


/*
* List all current child processes.
*/
void func_BGlist(char **cmd){
  // Print out all nodes in linked list
  // i.e. all child processes
  printList(head);
}


/*
* Send the SIGTERM signal to a child process
* to terminate it.
*/
void func_BGkill(char * str_pid){
  if(str_pid == NULL) {
    fprintf(stderr, "Please enter a PID.\n");
    return;
  }
  pid_t i_pid = atoi(str_pid);
  int if_exist = PifExist(head, i_pid);
  if(if_exist == 1) {
    int r_val = kill(i_pid, SIGTERM);
    if(r_val < 0) {
      fprintf(stderr, "Error: Process with PID %d could not be terminated.\n", i_pid);
    } else {
      printf("Process with PID %d has been terminated.\n", i_pid);
      head = deleteNode(head, i_pid);
      pid_t pid = waitpid(i_pid, NULL, WNOHANG);
      if(pid < 0) perror("waitpid");
    }
  } else {
    fprintf(stderr, "Error: Process with PID %d does not exist.\n", i_pid);
  }
}


/*
* Send the SIGSTOP signal to a child process
* to pause it.
*/
void func_BGstop(char * str_pid){
  if(str_pid == NULL) {
    fprintf(stderr, "Please enter a PID.\n");
    return;
  }
	pid_t i_pid = atoi(str_pid);
  int if_exist = PifExist(head, i_pid);
  if(if_exist) {
    int r_val = kill(i_pid, SIGSTOP);
    if(r_val < 0) {
      fprintf(stderr, "Error: Process with PID %d could not be stopped.\n", i_pid);
    }
  } else {
    fprintf(stderr, "Error: Process with PID %d does not exist.\n", i_pid);
  }
}


/*
* Send the SIGCONT signal to a child process
* to start it.
*/
void func_BGstart(char * str_pid){
  if(str_pid == NULL) {
    fprintf(stderr, "Please enter a PID.\n");
    return;
  }
	pid_t i_pid = atoi(str_pid);
  int if_exist = PifExist(head, i_pid);
  if(if_exist) {
    // Check status of child first
    int p_status;
    pid_t pid = waitpid(i_pid, &p_status, WNOHANG);
    if(pid < 0) perror("waitpid");
    if(WIFCONTINUED(p_status)) {
      printf("Process with PID %d has already been started.\n", i_pid);
      return;
    }
    
    int r_val = kill(i_pid, SIGCONT);
    if(r_val < 0) {
      fprintf(stderr, "Error: Process with PID %d could not be started.\n", i_pid);
    }
  } else {
    fprintf(stderr, "Error: Process with PID %d does not exist.\n", i_pid);
  }
}


/*
* See status information about a child process.
*/
void func_pstat(char * str_pid){
  pid_t i_pid = atoi(str_pid);
  int if_exist = PifExist(head, i_pid);
  if(if_exist) {
    // Initialize all variables
    char comm[50];
    char state;
    unsigned long utime, stime;
    long rss;
    unsigned long vctxt, nvctxt;

    // Open and read /proc/*pid*/stat
    char file_path[50];
    sprintf(file_path, "/proc/%d/stat", i_pid);
    FILE *f_stat = fopen(file_path, "r");
    if(f_stat == NULL) {
      printf("Error: Process with PID %d does not exist.\n", i_pid);
      return;
    }
    char * format_str = "%*d %s %c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu %ld %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*d %*d %*u %*u %*llu %*lu %*ld %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*d";
    fscanf(f_stat, format_str, &comm[0], &state, &utime, &stime, &rss);

    fclose(f_stat);

    // Format data from /proc/*pid*/stat
    char* comm_formatted = &comm[1];
    comm_formatted[strlen(comm) - 2] = '\0';
    float utime_div = (float) utime/sysconf(_SC_CLK_TCK);
    float stime_div = (float) stime/sysconf(_SC_CLK_TCK);
    printf("\ncomm: %s\nstate: %c\nutime: %f\nstime: %f\nrss: %ld\n", comm_formatted, state, utime_div, stime_div, rss);

    // Open and read /proc/*pid*/status
    sprintf(file_path, "/proc/%d/status", i_pid);
    FILE *f_status = fopen(file_path, "r");
    if(f_status == NULL) {
      printf("Error: Process with PID %d does not exist.\n", i_pid);
      return;
    }
    char buffer[200];
    
    // Print out the context switches lines only
    while(fgets(buffer, 200, f_status) != NULL) {
      if(strncmp(buffer, "voluntary_ctxt_switches", 23) == 0) {
        sscanf(buffer, "%*s %lu", &vctxt);
        printf("voluntary ctxt switches: %lu\n", vctxt);
      } else if(strncmp(buffer, "nonvoluntary_ctxt_switches", 26) == 0) {
        sscanf(buffer, "%*s %lu", &nvctxt);
        printf("nonvoluntary ctxt switches: %lu\n\n", nvctxt);
      }
    }

    fclose(f_status);

  } else {
    fprintf(stderr, "Error: Process with PID %d does not exist.\n", i_pid);
  }
}

/*
* Check for child processes which have
* terminated and notify user.
*/
void check_allStatus() {
  int p_status;
  pid_t pid = waitpid(-1, &p_status, WNOHANG);
  if(pid > 0){
    if(WIFEXITED(p_status)) {
      printf("\nProcess with PID %d has exited.\n\n", pid);
      head = deleteNode(head, pid);
    }
    if(WIFSIGNALED(p_status)) {
      printf("\nProcess with PID %d has been terminated.\n\n", pid);
      head = deleteNode(head, pid);
    }
  }
}

 
int main(){
    char user_input_str[50];
    while (true) {
      // Check status of child processes
      // and remove any that have terminated.
      check_allStatus();
      
      printf("Pman: > ");
      fgets(user_input_str, 50, stdin);
      char * ptr = strtok(user_input_str, " \n");
      if(ptr == NULL){
        continue;
      }

      char * lst[50];
      int index = 0;
      lst[index] = ptr;
      index++;
      while(ptr != NULL){
        ptr = strtok(NULL, " \n");
        lst[index]=ptr;
        index++;
      }
      
      
      if (strcmp("bg",lst[0]) == 0){
        func_BG(lst);
      } else if (strcmp("bglist",lst[0]) == 0) {
        func_BGlist(lst);
      } else if (strcmp("bgkill",lst[0]) == 0) {
        func_BGkill(lst[1]);
      } else if (strcmp("bgstop",lst[0]) == 0) {
        func_BGstop(lst[1]);
      } else if (strcmp("bgstart",lst[0]) == 0) {
        func_BGstart(lst[1]);
      } else if (strcmp("pstat",lst[0]) == 0) {
        func_pstat(lst[1]);
      } else if (strcmp("q",lst[0]) == 0) {
        free_allNodes(head);
        printf("Bye Bye \n");
        exit(0);
      } else {
        printf("Invalid input\n");
      }
    }

  return 0;
}

