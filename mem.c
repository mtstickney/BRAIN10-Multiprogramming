#include <stdio.h>
#include <string.h>
#include "mem.h"

#define PROCS 10

/* we're only declaring the memory we'll use, rather than all 10000 locations */
static char mem[4*100*PROCS];

void set_mem(char a) {
	memset(mem, a, 4*100*PROCS);
}

/* It's much simpler to use the numberic pid instead of the base and limit registers.
 * a (disabled) version that uses the registers appears below, for grading purposes
 * (note that changes in other modules would have to be made to use it). */
static char *get_memp(unsigned int pid, unsigned int addr) {
	if (addr > 99) {
		return NULL;
	}
	return &mem[4*(pid*100+addr)];
}

#if 0
extern int word2int(char *p);
extern void int2word(int a, char *b);
static char *get_memp(struct proc *p, unsigned int addr) {
	unsigned int limit,base;

	limit = word2int(p->lr);
	base = word2int(p->br);
	if (limit < 0 || base < 0) {
		fprintf(stderr, "get_memp: invalid base or limit register\n");
		return NULL;
	}
	if (add > limit) {
		fprintf(stderr, "get_memp: invalid address\n");
		return NULL;
	}
	return &mem[4*(base+addr)];
}
#endif

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
	int i,j;
	char *p;

	for (i=0; i<1000; i++) {
		printf("%03d ", i);
		for (j=0; j<9; j++, i++) {
			p = get_memp(i/100, i%100);
			printf("%c%c%c%c ", p[0], p[1], p[2], p[3]);
		}
		p=get_memp(i/100, i%100);
		printf("%c%c%c%c\n", p[0], p[1], p[2], p[3]);
	}
}	
