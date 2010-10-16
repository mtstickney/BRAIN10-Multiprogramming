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

static int check_header(char *word, int *loc)
{
	char buf[5];
	size_t len;
	size_t pid, offset;
	int c;

	scanf("%4s", buf);
	len = strlen(buf);
	if (len == 4) { /* normal word */
		pid = *loc/100;
		offset = *loc%100;
		*loc += 2;
		if (store(pid, word, offset++) != 0) {
			fprintf(stderr, "check_header: store failed\n");
			return 1;
		}
		if (offset > 99) {
			fprintf(stderr, "check_header: read more than 99 words without seeing a BRAIN10 header\n");
			return 1;
		}
		if (store(pid, buf, offset) != 0) {
			fprintf(stderr, "check_header: store failed\n");
			return 1;
		}
	} else if (strcasecmp(buf, "N10") == 0 && len == 3) { /* BRAIN10 header */
		if (*loc > 0) {
			/* fast forward to end of address space */
			*loc += 99 - (*loc)%100;
		}
		return 0;
	} else {
		do {
			c = fgetc(stdin);
			if (!isspace(c))
				buf[len++] = c;
		} while(len < 4 && !feof(stdin));
		if (feof(stdin)) {
			fprintf(stderr, "check_header: Unexpected EOF\n");
			return 1;
		}
		return 0;
	}
}

static int store_word(char *word, int *loc)
{
	size_t pid, offset;

	pid = *loc/100;
	offset = *loc%100;
	(*loc)++;
	return store(pid, word, offset);
}

/* notes: entries are checked in order, first that matches wins.
	NULL string matches any word, NULL action terminates.
	only the first 4 chars are checked. */
struct str_act {
	char *match;
	int (*action)(char *word, int *loc);
};

static struct str_act parse_table[]=
{
	{"BRAI", check_header},
	{"DATA", NULL},
	{NULL, store_word},
};

int load_progs()
{
	struct str_act *p;
	int i,addr;
	char buf[5];

	addr=0;
	while (addr<1000) {
		read_word(buf);
		p = parse_table;
		for (i=0; i<LEN(parse_table); i++) {
			if (p->match == NULL || strncasecmp(buf, p->match, 4) == 0)
				break;
			p++;
		}
		if (i >= LEN(parse_table)) {
			fprintf(stderr, "load_progs: no matching action for word\n");
			return 1;
		}
		if (p->action == NULL) {
			while (fgetc(stdin) != '\n'); /* ignore the rest of the line */
			return 0;
		}
		if (p->action(buf, &addr) != 0) {
			fprintf(stderr, "load_progs: action failed\n");
			return 1;
		}
		while (fgetc(stdin) != '\n');
	}
	return 0;
}

int main()
{
	struct proc p;

	set_mem('0');
	if (load_progs() != 0)
		return 1;

	/* set up the process */
	memset(&p, '0', sizeof(struct proc));
	p.c = 'F';
	p.runnable = 1;
	p.stack_base = 0;
	p.pid = 0;

	print_mem();
	while (tick(&p) == 0 && p.runnable);
	print_mem();

	return 0;
}
