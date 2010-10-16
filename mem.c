#include <stdio.h>
#include <string.h>
#include "mem.h"

#define PROCS 10

static char mem[4*100*PROCS];

void set_mem(char a) {
	memset(mem, a, 4*100*PROCS);
}

static char *get_memp(unsigned int pid, unsigned int addr) {
	if (addr > 99) {
		return NULL;
	}
	return &mem[pid*100+4*addr];
}

/* Note: load and store convert endianness of words. */
int load(unsigned int pid, unsigned int addr, char *dest) {
	void *src;

	src = get_memp(pid, addr);
	if (src == NULL) {
		fprintf(stderr, "load: invalid address %d\n", addr);
		return -1;
	}
	memcpy(dest, src, 4);
	return 0;
}

int store(unsigned int pid, const char *src, unsigned int addr)
{
	char *dest;

	dest = get_memp(pid, addr);
	if (dest == NULL) {
		fprintf(stderr, "store: invalid address %d\n", addr);
		return -1;
	}

	memcpy(dest, src, 4);
	return 0;
}

void print_mem()
{
	int i,j,k;
	char *p;
	
	for (k=0; k<10; k++) {
		for (i=0; i<100; i++) {
			printf("%02d ", i);
			for (j=0; j<9; j++, i++) {
				p = get_memp(k, i);
				printf("%c%c%c%c ", p[0], p[1], p[2], p[3]);
			}
			p=get_memp(k, i);
			printf("%c%c%c%c\n", p[0], p[1], p[2], p[3]);
		}
	}
}	
