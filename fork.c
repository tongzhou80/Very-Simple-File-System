#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/wait.h>

void execfs() {
  //arguments for ls, will run: ls  -l /bin
  char * ls_args[4] = { "ls", "-l", "/bin", NULL} ;
  pid_t c_pid, pid;
  int status;

  c_pid = fork();

  if (c_pid == 0){
    /* CHILD */

    printf("Child: executing ls\n");

    //execute ls
    execvp( ls_args[0], ls_args);
    //only get here if exec failed
    perror("execve failed");
  }else if (c_pid > 0){
    /* PARENT */

    if( (pid = wait(&status)) < 0){
      perror("wait");
      _exit(1);
    }

    printf("Parent: finished\n");

  }else{
    perror("fork failed");
    _exit(1);
  }
  
}

int main(int argc, char * argv[]){
  execfs();
  return 0; //return success
}
