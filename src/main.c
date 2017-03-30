#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

int main(int argc, char * const argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Not enough arguments\n");
        return 1;
    }
	
	int n = 0;
	switch (getopt(argc, argv, "p:")) {
		case 'p':
			n = atoi(optarg);
			if (!n || n <= 0 || n > 10) {
				fprintf(stderr, "p parameter must be int from 1 to 10\n");
				return 1;
			}
			break;
		case -1:
			return 1;
	}
	
	printf("%d \n",n);
	
	return 0;
}
