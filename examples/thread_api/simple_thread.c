#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *mythread(void *arg)
{
    int m = (int)arg;
    printf("arg:%d\n", m);
    return (void *)(arg + 1);
}

int main(int argc, char *argv[])
{
    pthread_t p;
    int rc, ret;

    rc = pthread_create(&p, NULL, mythread, (void *)100);
    assert(rc == 0);
    rc = pthread_join(p, (void **)&ret);
    assert(rc == 0);
    printf("ret: %d\n", ret);
    return 0;
}