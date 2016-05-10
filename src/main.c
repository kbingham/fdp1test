/*
 * main.c
 *      Author: kbingham
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/prctl.h>


void help(char ** argv)
{
	char * searched = strrchr(argv[0], '/');
	char * appname = searched ? searched + 1 : argv[0];

	printf("%s: \n", appname);

	printf("\n");
}

int process_arguments(int argc, char ** argv)
{
	int option;

	static struct option long_options[] = {
			/*  { .name, .has_arg, .flag, .val } */
			{"help", no_argument, 0, 'h'},

			{0, 0, 0, 0}
	};

	while ((option = getopt_long(argc, argv,
			"h",
			long_options, NULL)) != -1) {

		switch (option) {

		default:
		case 'h':
			help(argv);
			exit(0);
			break;
		}

	}
	return 0;

}

int main(int argc, char ** argv)
{
    prctl(PR_SET_NAME, "main");

    process_arguments(argc, argv);


	return 0;
}
