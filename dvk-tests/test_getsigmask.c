#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ptrace.h>

int
main(int argc, char *argv[])
{
    pid_t pid, err;
    int status;
	sigset_t ss;
	sigset_t *ss_ptr;


   pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

   if (pid == 0) {            /* Code executed by child */
        printf("Child PID is %ld\n", (long) getpid());
		err = ptrace(PTRACE_TRACEME, getpid(), 0, 0);
		while(1) {
			if (argc == 1) pause();                    /* Wait for signals */
		}
        _exit(atoi(argv[1]));

   } else {                    /* Code executed by parent */
        do {
			err = waitpid(pid, &status, WUNTRACED | __WALL);		
            if (err == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

           if (WIFEXITED(status)) {
                printf("exited, status=%d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("killed by signal %d\n", WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                printf("stopped by signal %d\n", WSTOPSIG(status));
            } else if (WIFCONTINUED(status)) {
                printf("continued\n");
            }
			ss_ptr = &ss;
			err = ptrace(PTRACE_GETSIGMASK, pid, sizeof(sigset_t), ss_ptr);
			if (err < 0) {
				printf("handle_dvk_wait - PTRACE_GETSIGMASK failed, errno = %d\n", errno);
				exit(1);
			}
			
			err = sigaddset(ss_ptr, SIGUSR1);
			if (err < 0) {
				printf( "handle_dvk_wait - sigaddset failed, errno = %d\n", errno);
				exit(1);
			}
			
			err = ptrace(PTRACE_SETSIGMASK, pid, sizeof(sigset_t), ss_ptr);
			if (err < 0) {
				printf( "handle_dvk_wait - PTRACE_SETSIGMASK failed, errno = %d\n", errno);
				exit(1);
			}
			err = ptrace(PTRACE_CONT, pid, 0, 0);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        exit(EXIT_SUCCESS);
    }
}

