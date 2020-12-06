#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

void *func(void *vargp)
{
        sleep(60);

        return 0;
}

int main(int argc, char **argv)
{
        pthread_t tid;

        pthread_create(&tid, NULL, func, NULL);
        pthread_join(tid, NULL);

        return 0;
}
