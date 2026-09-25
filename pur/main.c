#include <panuti/handle.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int klfd = handle_open("/dvc/kbd/line");
	if (klfd < 0) {
		printf("opening keyboard line mode failed\n");
		return -1;
	}

	while (1) {
		printf("# ");
		
		char buf[256];
		int n = handle_read(klfd, buf, sizeof(buf));
		if (n < 0) {
			printf("read from keyboard failed\n");
			return -1;
		}

		printf("%.*s\n", n, buf);
	}
}