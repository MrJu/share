#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>

int main(int argc, char **argv)
{
        int ret;
        char name[64];

        ret = fork();
        if (ret == 0) {
                prctl(PR_SET_NAME, "bar", NULL, NULL, NULL);
                prctl(PR_GET_NAME, name);
                printf("Child pid = %d, name: %s\n", getpid(), name);
        } else {
                prctl(PR_SET_NAME, "foo", NULL, NULL, NULL);
                prctl(PR_GET_NAME, name);
                printf("Parent pid =  %d, name: %s\n", getpid(), name);
        }

        return 0;
}
