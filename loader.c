#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "mem.h"
#include "vm.h"

/* note: buf should be able to hold word_size+1 bytes, since we use scanf to read */
void read_word(char *buf)
{
	size_t len;
	int c;

	len = 0;
	if (scanf("%4s", buf) != 1 || (len=strlen(buf)) < 4) {
		/* short or failed read */
		fprintf(stderr, "read_word: short or failed read, trying to recover\n");

		/* pull the remaining non-whitespace chars */
		do {
			c = fgetc(stdin);
			if (!isspace(c))
				buf[len++] = c;
		} while (len < 4 && !feof(stdin));
	}
}
	

int load_file()	//Loads brain10 file into memory
{
	char buf[8];
	int i, c;

	/* clear memory for printing purposes */
	set_mem('0');

	/* check for header */
	while (isspace(c=fgetc(stdin)));
	ungetc(c, stdin);
	if (scanf("%7s", buf) != 1 || strncasecmp(buf, "BRAIN10", 7) != 0) {
		fprintf(stderr, "warning: missing or incorrect file header, may not be a BRAIN10 program\n");
	}

	/* consume whitespace */
	while (isspace(c=fgetc(stdin)));
	ungetc(c, stdin);
	

	for (i=0; i<100; i++) {
		read_word(buf);
		if (feof(stdin)) {
			fprintf(stderr, "load: Unexpected EOF. Attempting to run, expect badness.\n");
			return 0;
		}

		if (strncasecmp(buf, "DATA", 4) == 0)
			return 0;
		/* now that we have 4 chars, save 'em */
		if (store(buf, i) == -1) {
			fprintf(stderr, "load: store failed\n");
			return -1;
		}

		/* ignore the rest of the line */
		while (fgetc(stdin) != '\n');
	}

	/* consume leftover instructions */
	do {
		read_word(buf);
	} while (strncasecmp(buf, "DATA", 4) != 0);
	if (feof(stdin)) 
		fprintf(stderr, "load: unexpected EOF, expect badness.\n");
	return 0;
}

int main()
{
	struct proc p;

	if (load_file() == -1)
		return 1;

	/* set up the process */
	memset(&p, '0', sizeof(struct proc));
	p.c = 'F';
	p.runnable = 1;
	p.stack_base = 0;

	print_mem();
	while (tick(&p) == 0 && p.runnable);
	print_mem();

	return 0;
}
