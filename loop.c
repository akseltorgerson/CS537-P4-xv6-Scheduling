#include "types.h"
#include "user.h"

int main(int argc, char **argv) {

	sleep(atoi(argv[1]));

	int i = 0, j = 0;
	while (i < 80000000) {
		j += i * j + 1;
		i++;
	}

	exit();
	return 0;
}
