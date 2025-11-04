#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    printf("hello world (pid: %d)\n", (int)getpid());
    int rc = fork();
    if (rc < 0)
    {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        printf("hello, I am child (pid: %d)\n", (int)getpid());
    }
    else
    {
        int *child_exit_code = (int *)malloc(sizeof(int));
        int rc_wait = waitpid(rc, child_exit_code, 0);
        printf("hello, I am parent of %d, (rc_wait: %d), (child_exit_code: %d), (pid: %d)\n", rc, rc_wait, (int)*child_exit_code, (int)getpid());
        free(child_exit_code);
    }
    return 0;
}