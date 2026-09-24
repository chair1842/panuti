#include <stdio.h>
#include <panuti/mount.h>
#include <panuti/process.h>
#include <sys/types.h>
#include <panuti/syscall/syscallsf.h>

int main(int argc, char** argv) {
	if (panutisysf_mkdir("/cd") != 0) {
		printf("creating /cd failed\n");
		return -1;
	}
	
	if (mount("/cd", "isofs", "/dvc/cdrom0") != 0) {
		printf("mounting /dvc/cdrom0 on /cd with fs \"isofs\" failed\n");
		return -1;
	}
	
	while (1) {
		char* argv = "pur"; // "name" of the program
		pid_t pid = procreate("/cd/usr/bin/pur", &argv, 1, NULL, 0, NULL, 0);
		if ((int32_t)pid < 0) {
			printf("procreating /cd/usr/bin/pur failed\n");
			return -1;
		}

		int ec = 0;
		if (wait(pid, &ec) != 0) {
			printf("waiting on pur failed\n");
			return -1;
		}
	}
} 