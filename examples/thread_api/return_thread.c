#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct __myarg_t
{
    int a;
    int b;
} myarg_t;

typedef struct __myreturn_t
{
    int c;
} myreturn_t;

void *mythread(void *arg)
{
    myarg_t *m = (myarg_t *)arg;
    printf("a:%d, b:%d\n", m->a, m->b);
    myreturn_t *r = (myreturn_t *)malloc(sizeof(myreturn_t));
    assert(r != NULL);
    r->c = m->a + m->b;
    return (void *)r;
}

int main(int argc, char *argv[])
{
    pthread_t p;
    myreturn_t *ret;
    int rc;

    myarg_t args = {42, 69};
    rc = pthread_create(&p, NULL, mythread, &args);
    assert(rc == 0);
    rc = pthread_join(p, (void **)&ret);
    assert(rc == 0);
    printf("c: %d\n", ret->c);
    free(ret);
    return 0;
}