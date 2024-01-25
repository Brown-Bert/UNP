#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstdlib>

struct res{
    int value;
};

void* func(void*)
{
    sleep(5);
    puts("退出");
    struct res *r = new(struct res);
    r->value = 15;
    pthread_exit(r);
}

int main(int argc, char* argv[])
{
    pthread_t tid;
    pthread_create(&tid, NULL, func, NULL);
    puts("4543");
    void *value;
    pthread_join(tid, &value);
    struct res *r = (struct res*)value;
    printf("value = %d\n", r->value);
    free(r);
    puts("4543");
    exit(0);
}