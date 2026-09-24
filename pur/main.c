#include <panuti/handle.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int klfd = handle_open("/dvc/kbd/line");
	if (klfd < 0) {
		printf("opening keyboard line mode failed\n");
		return -1;
	}

	while (1) {
		char buf[256];
		if (handle_read(klfd, buf, 256) < 0) {
			printf("read from keyboard failed\n");
			return -1;
		}

		printf(buf);
	}
}