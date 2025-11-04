#include <pthread.h>
#include <stdio.h>

typedef struct __myarg_t
{
    int a;
    int b;
} myarg_t;

void *mythread(void *arg)
{
    myarg_t *m = (myarg_t *)arg;
    printf("a:%d, b:%d\n", m->a, m->b);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t p;
    int rc;

    myarg_t args;
    args.a = 42;
    args.b = 69;
    rc = pthread_create(&p, NULL, mythread, &args);
    rc = pthread_join(p, NULL);
}