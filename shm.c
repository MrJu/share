#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/shm.h>
#define SEGMENT_SIZE (1024 * 1024 * 100)
void *sh_mem[1024];

int main(int argc, char **argv)
{
	int status;
	int segment_id;
	int i;
	pid_t pid;

	system("echo m > /proc/sysrq-trigger");

	segment_id = shmget (IPC_PRIVATE, SEGMENT_SIZE,
				IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

	for (i = 0; i < 1024; i++) {
		pid = fork();
		if (!pid) {
			sh_mem[i] = shmat(segment_id, NULL, 0);
			printf("%s: %d segment_id:%d sh_mem[%d]:%p\n",
					__func__, __LINE__, segment_id, i, sh_mem);
			memset(sh_mem[i], 0xff, SEGMENT_SIZE);

			sleep(3600);
			shmdt(sh_mem[i]);
			exit(EXIT_SUCCESS);
		}
	}

	system("echo m > /proc/sysrq-trigger");

	sleep(120);

	shmctl(segment_id, IPC_RMID, 0);
	exit(EXIT_SUCCESS);

}

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/shm.h>

#define SEGMENT_SIZE (1024 * 1024 * 100)
void *sh_mem[1024];

int main(int argc, char **argv)
{
	int status;
	int segment_id;
	int i;

	system("echo m > /proc/sysrq-trigger");

	segment_id = shmget (IPC_PRIVATE, SEGMENT_SIZE,
				IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

	for (i = 0; i < 1024; i++) {
		sh_mem[i] = shmat(segment_id, NULL, 0);
		printf("%s: %d segment_id:%d sh_mem[%d]:%p\n",
				__func__, __LINE__, segment_id, i, sh_mem);

		memset(sh_mem[i], 0xff, SEGMENT_SIZE);
	}

	system("echo m > /proc/sysrq-trigger");
	sleep(120);

	for (i = 0; i < 1024; i++)
		shmdt(sh_mem[i]);

	shmctl(segment_id, IPC_RMID, 0);
	exit(EXIT_SUCCESS);
}

