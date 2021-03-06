#include <string.h>
#include <stdio.h>
#include "vm.h"
#include "mem.h"
#include "sched.h"
#include "wait_queue.h"

struct msg_queues
{
	struct plist_head senders;
	struct plist_head recvers;
};

/* wait queues for message passing */
/* Note: last position is used to store procs receiving from any */
static struct msg_queues wq_table[11];

void msg_init()
{
	memset(wq_table, 0, 10*sizeof(struct msg_queues));
}

int word2int(char *p)
{
	int i,j;

	i=0;
	for (j=0; j<4; j++) {
		if (p[j] < '0' || p[j] > '9')
			return -1;
		i = i*10+p[j]-'0';
	}
	return i;
}

void int2word(int a, char *p)
{
	int i;

	for (i=3; i>=0; i--) {
		p[i] = (a % 10) + '0';
		a /= 10;
	}
}

static int load_register(struct proc *p, int addr)
{
	 if (addr < 0 || addr > 99) {
		fprintf(stderr, "load_register: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, p->r) == -1) {
		fprintf(stderr, "load_register: load failed\n");
		return -1;
	}
	return 0;
}

static int load_low(struct proc *p, int addr)
{
	char temp[4];

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "load_low: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "load_low: load failed\n");
		return -1;
	}
	memcpy(p->r+2, temp+2, 2);
	return 0;
}

static int load_high(struct proc *p, int addr)
{
	char temp[4];

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "load_high: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "load_high: load failed\n");
		return -1;
	}
	memcpy(p->r, temp, 2);
	return 0;
}

static int store_register(struct proc *p, int addr)
{
	if (addr < 0 || addr > 99) {
		fprintf(stderr, "store_register: invalid address\n");
		return -1;
	}
	if (store(p->pid, p->r, addr) == -1) {
		fprintf(stderr, "store_register: store failed\n");
		return -1;
	}
	return 0;
}

static int set_sp(struct proc *p, int addr)
{
	addr = word2int(p->r);
	if (addr < 0 || addr > 99) {
		fprintf(stderr, "set_sp: invalid address in register\n");
		return -1;
	}
	p->stack_base = addr;
	memcpy(p->sp, p->r, 4);
	return 0;
}

static int get_sp(struct proc *p, int addr)
{
	memcpy(p->r, p->sp, 4);
	return 0;
}

static int push(struct proc *p, int addr)
{
	addr = word2int(p->sp);
	if (addr == -1) {
		fprintf(stderr, "push: Non-numeric stack pointer\n");
		return -1;
	}
	addr++;
	int2word(addr, p->sp);
	if (addr > 99) {
		fprintf(stderr, "push: Out of memory\n");
		return -1;
	}
	if (store(p->pid, p->r, addr) == -1) {
		fprintf(stderr, "push: store failed\n");
		return -1;
	}
	return 0;
}

static int pop(struct proc *p, int addr)
{
	addr = word2int(p->sp);
	if (addr == -1) {
		fprintf(stderr, "pop: non-numeric stack pointer\n");
		return -1;
	}
	if (addr > 99) {
		fprintf(stderr, "pop: Invalid stack pointer\n");
		return -1;
	}
	if (addr <= p->stack_base) {
		fprintf(stderr, "pop: pop on empty stack\n");
		return -1;
	}
	if (load(p->pid, addr, p->r) == -1) {
		fprintf(stderr, "pop: load failed\n");
		return -1;
	}
	addr--;
	int2word(addr, p->sp);
	return 0;
}
	
static int cmp_eql(struct proc *p, int addr)
{
	char temp[4];

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "cmp_eql: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "cmp_eql: load failed\n");
		return -1;	
	}
	if (memcmp(p->r, temp, 4) == 0)
		p->c = 'T';
	else
		p->c = 'F';
	return 0;
}

static int cmp_less(struct proc *p, int addr)
{
	char temp[4];

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "cmp_less: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "cmp_less: load failed\n");
		return -1;
	}
	if (memcmp(p->r, temp, 4) < 0)
		p->c = 'T';
	else
		p->c = 'F';
	return 0;
}

static int jmp_if(struct proc *p, int addr)
{
	char temp[4];

	if (addr < 0 || addr > 99)
		return -1;

	int2word(addr, temp);
	if (p->c == 'T') {
		memcpy(p->ic, temp+2, 2);
	}
	return 0;
}

static int jmp(struct proc *p, int addr)
{
	char temp[4];

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "jmp: invalid address\n");
		return -1;
	}
	int2word(addr, temp);
	memcpy(p->ic, temp+2, 2);
	return 0;
}

extern void read_word(char *buf);

static int read(struct proc *p, int addr)
{
	char temp[5];
	int i;

	if (addr < 0 || addr > 89) {
		fprintf(stderr, "read: invalid addresss\n");
		return -1;
	}
	for (i=0; i<10; i++) {
		read_word(temp);
		/* FIXME: this fails to read full words that begin with 'END' */
		if (strncasecmp(temp, "END", 3) == 0) {
			fprintf(stderr, "read: Out of input data\n");
			return -1;
		}
		if (feof(stdin)) {
			fprintf(stderr, "read: Unexpected EOF\n");
			return -1;
		}
		if (store(p->pid, temp, addr++) == -1) {
			fprintf(stderr, "read: store failed\n");
			return -1;
		}
	}
	return 0;
}

static int print(struct proc *p, int addr)
{
	char temp[4];
	int i;

	if (addr < 0 || addr > 89) {
		fprintf(stderr, "print: invalid address\n");
		return -1;
	}

	for (i=0; i<10; i++) {
		if (load(p->pid, addr++, temp) == -1) {
			fprintf(stderr, "print: load failed\n");
			return -1;
		}
		printf("%c%c%c%c\n", temp[0], temp[1], temp[2], temp[3]);
	}
	return 0;
}

static int add(struct proc *p, int addr)
{
	int i,j;
	char temp[4];

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "add: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "add: load failed\n");
		return -1;
	}
	i = word2int(temp);
	j = word2int(p->r);
	if (i == -1) {
		fprintf(stderr, "add: non-numeric word in memory\n");
		return -1;
	}
	if (j == -1) {
		fprintf(stderr, "add: non-numeric word in register\n");
		return -1;
	}
	i += j;
	int2word(i, p->r);
	return 0;
}

static int subtract(struct proc *p, int addr)
{
	char temp[4];
	int i,j;

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "subtract: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "subtract: load failed\n");
		return -1;
	}
	
	i = word2int(p->r);
	j = word2int(temp);
	if (i == -1) {
		fprintf(stderr, "subtract: non-numeric word in register\n");
		return -1;
	}
	if (j == -1) {
		fprintf(stderr, "subtract: non-numeric word in memory\n");
		return -1;
	}
	if (i < j) {
		fprintf(stderr, "subtract: negative result\n");
		return -1;
	}
	i -= j;
	int2word(i, p->r);
	return 0;
}

static int multiply(struct proc *p, int addr)
{
	char temp[4];
	int i,j;

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "multiply: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "multiply: load failed\n");
		return -1;
	}

	i = word2int(p->r);
	j = word2int(temp);
	if (i == -1) {
		fprintf(stderr, "multiply: non-numeric word in register\n");
		return -1;
	}
	if (j == -1) {
		fprintf(stderr, "multiply: non-numeric word in memory\n");
		return -1;
	}

	i *= j;
	int2word(i, p->r);
	return 0;
}

static int divide(struct proc *p, int addr)
{
	char temp[4];
	int i,j;

	if (addr < 0 || addr > 99) {
		fprintf(stderr, "divide: invalid address\n");
		return -1;
	}
	if (load(p->pid, addr, temp) == -1) {
		fprintf(stderr, "divide: load failed\n");
		return -1;
	}

	i = word2int(p->r);
	j = word2int(temp);
	if (i == -1) {
		fprintf(stderr, "divide: non-numeric word in register\n");
		return -1;
	}
	if (j == -1) {
		fprintf(stderr, "divide: non-numeric word in memory\n");
		return -1;
	}
	if (j == 0) {
		fprintf(stderr, "divide: division by zero\n");
		return -1;
	}
	i /= j;
	int2word(i, p->r);
	return 0;
}

static int add_stack(struct proc *p, int addr)
{
	char word1[4];
	char word2[4];
	int i,j;

	addr = word2int(p->sp);
	if (addr < 0 || addr > 99) {
		fprintf(stderr, "add_stack: invalid stack pointer\n");
		return -1;
	}
	if (addr < p->stack_base+1) {
		fprintf(stderr, "add_stack: stack contains less than 2 elements\n");
		return -1;
	}

	if (load(p->pid, addr--, word1) == -1) {
		fprintf(stderr, "add_stack: load failed\n");
		return -1;
	}
	if (load(p->pid, addr, word2) == -1) {
		fprintf(stderr, "add_stack: load failed\n");
		return -1;
	}
	i = word2int(word1);
	j = word2int(word2);
	if (i == -1 || j == -1) {
		fprintf(stderr, "add_stack: non-numeric word on stack\n");
		return -1;
	}

	j += i;
	int2word(j, word2); /* store new stack value */
	int2word(addr, p->sp); /* store stack pointer */
	if (store(p->pid, word2, addr) == -1) {
		fprintf(stderr, "add_stack: store failed\n");
		return -1;
	}
	return 0;
}

static int subtract_stack(struct proc *p, int addr)
{
	char word1[4];
	char word2[4];
	int i,j;

	addr = word2int(p->sp);
	if (addr < 0 || addr > 99) {
		fprintf(stderr, "subtract_stack: invalid stack pointer\n");
		return -1;
	}
	if (load(p->pid, addr--, word1) == -1) {
		fprintf(stderr, "subtract_stack: load failed\n");
		return -1;
	}
	if (load(p->pid, addr, word2) == -1) {
		fprintf(stderr, "subtract_stack: load_failed\n");
		return -1;
	}

	i = word2int(word1);
	j = word2int(word2);
	if (i==-1 || j==-1) {
		fprintf(stderr, "subtract_stack: non-numeric word on stack\n");
		return -1;
	}
	if (i > j) {
		fprintf(stderr, "subtract_stack: negative result\n");
		return -1;
	}

	j -= i;
	int2word(j, word2);
	int2word(addr, p->sp);
	if (store(p->pid, word2, addr) == -1) {
		fprintf(stderr, "subtract_stack: store failed\n");
		return -1;
	}
	return 0;
}

static int multiply_stack(struct proc *p, int addr)
{
	char word1[4];
	char word2[4];
	int i,j;

	addr = word2int(p->sp);
	if (addr < 0 || addr > 99) {
		fprintf(stderr, "multiply_stack: invalid stack pointer\n");
		return -1;
	}
	if (load(p->pid, addr--, word1) == -1) {
		fprintf(stderr, "multiply_stack: load failed\n");
		return -1;
	}
	if (load(p->pid, addr, word2) == -1) {
		fprintf(stderr, "multiply_stack: load failed\n");
		return -1;
	}

	i = word2int(word1);
	j = word2int(word2);
	if (i==-1 || j==-1) {
		fprintf(stderr, "multiply_stack: non-numeric word on stack\n");
		return -1;
	}

	j *= i;
	int2word(j, word2);
	int2word(addr, p->sp);
	if (store(p->pid, word2, addr) == -1) {
		fprintf(stderr, "multiply_stack: store failed\n");
		return -1;
	}
	return 0;
}

static int divide_stack(struct proc *p, int addr)
{
	char word1[4];
	char word2[4];
	int i,j;

	addr = word2int(p->sp);
	if (addr < 0 || addr > 99) {
		fprintf(stderr, "divide_stack: invalid stack pointer\n");
		return -1;
	}
	if (load(p->pid, addr--, word1) == -1) {
		fprintf(stderr, "divide_stack: load failed\n");
		return -1;
	}
	if (load(p->pid, addr, word2) == -1) {
		fprintf(stderr, "divide_stack: load failed\n");
		return -1;
	}

	i = word2int(word1);
	j = word2int(word2);

	if (i==-1 || j==-1) {
		fprintf(stderr, "divide_stack: non-numeric word on stack\n");
		return -1;
	}
	if (i == 0) {
		fprintf(stderr, "divide_stack: division by zero\n");
		return -1;
	}

	j /= i;
	int2word(j, word2);
	int2word(addr, p->sp);
	if (store(p->pid, word2, addr) == -1) {
		fprintf(stderr, "divide_stack: store failed\n");
		return -1;
	}
	return 0;
}

static int nop(struct proc *p, int addr)
{
	return 0;
}

static struct plist *get_recver(struct proc *sender, int target_pid)
{
	struct plist *waitp;

	/* check receievers waiting on us */
	for (waitp = wq_table[sender->pid].recvers.head; waitp != NULL; waitp = waitp->next) {
		if (waitp->p->pid == target_pid)
			break;
	}
	if (waitp != NULL)
		return waitp;
	/* check receivers waiting on anybody */
	for (waitp = wq_table[10].recvers.head; waitp != NULL; waitp = waitp->next) {
		if (waitp->p->pid == target_pid)
			break;
	}
	return waitp;
}

static int send(struct proc *p, int pid)
{
	struct plist *waitp;
	char word[4];
	unsigned int ic;

	if (pid < 0 || pid > 9) {
		fprintf(stderr, "send: invalid process id\n");
		return 1;
	}

	waitp = get_recver(p, pid);
	if (waitp == NULL) {
		/* nobody's waiting on us, add ourselves to the wait list */
		if (insert_proc(&(wq_table[pid].senders), p) != 0) {
			fprintf(stderr, "send: failed to add proc to wait queue\n");
			return 1;
		}
		sched_suspend(p->pid);
		return 0;
	}

	/* reset the receiver's IC to the read instruction and wake it up */
	memset(word, '0', 4);
	memcpy(word+2, waitp->p->ic, 2);
	ic = word2int(word);
	if (ic < 0 || ic > 99) {
		fprintf(stderr, "send: invalid IC in receiver process\n");
		return 1;
	}
	int2word(--ic, word);
	memcpy(waitp->p->ic, word+2, 2);
	/* put ourselves on the wait queue */
	if (insert_proc(&(wq_table[pid].senders), p) != 0) {
		fprintf(stderr, "send: failed to add self to wait queue\n");
		return 1;
	}
	/* remove receiver from whichever wait queue it was on */
	if (remove_proc(&(wq_table[p->pid].recvers), &proc_table[pid]) != 0 &&
	    remove_proc(&(wq_table[10].recvers), &proc_table[pid]) != 0) {
		fprintf(stderr, "send: Failed to remove receiver from wait queue\n");
		return 1;
	}
	sched_resume(pid);
	sched_suspend(p->pid);
	return 0;
}

static int recv(struct proc *p, int pid)
{
	struct plist *waitp;
	int store_loc, load_loc;
	int i;
	char temp[4];

	if (pid > 9) {
		fprintf(stderr, "recv: invalid pid\n");
		return 1;
	}

	store_loc = word2int(p->r);
	if (store_loc < 0) {
		fprintf(stderr, "recv: invalid storage location\n");
		return 1;
	}

	/* get the waiting process, if any */
	if (pid < 0) {
		/* receive from any */
		waitp = wq_table[p->pid].senders.head;
	} else {
		for (waitp=wq_table[p->pid].senders.head; waitp!=NULL; waitp=waitp->next) {
			if (waitp->p->pid == pid)
				break;
		}
	}
	if (waitp == NULL) {
		/* put ourselves on wait queue and suspend */
		if (pid < 0 && insert_proc(&(wq_table[10].recvers), p) != 0) {
			fprintf(stderr, "recv: Failed to add self to wait queue\n");
			return 1;
		}
		if (pid >= 0 && insert_proc(&(wq_table[pid].recvers), p) != 0) {
			fprintf(stderr, "recv: Failed to add self to wait queue\n");
			return 1;
		}
		sched_suspend(p->pid);
		return 0;
	}

	load_loc = word2int(waitp->p->r);
	if (load_loc < 0) {
		fprintf(stderr, "recv: invalid load location\n");
		return 1;
	}
	for (i=0; i<10; i++) {
		if (load(waitp->p->pid, load_loc++, temp) != 0) {
			fprintf(stderr, "recv: load failed\n");
			return 1;
		}
		if (store(p->pid, temp, store_loc++) != 0) {
			fprintf(stderr, "recv: store failed\n");
			return 1;
		}
	}
	/* we're done with the sender, so wake them up */
	pid = waitp->p->pid;
	if (remove_proc(&(wq_table[p->pid].senders), waitp->p) != 0) {
		fprintf(stderr, "recv: failed to remove sender from wait queue\n");
		return 1;
	}
	sched_resume(pid);
	return 0;
}

static int identify(struct proc *p, int addr)
{
	int2word(p->pid, p->r);
	return 0;
}

static int halt(struct proc *p, int addr)
{
	sched_reset(p->pid);
	sched_suspend(p->pid);
	return 0;
}

static struct op op_table[] = {
	{ .opcode=LR, .run=load_register },
	{ .opcode=LL, .run=load_low },
	{ .opcode=LH, .run=load_high },
	{ .opcode=SR, .run=store_register },
	{ .opcode=SP, .run=set_sp },
	{ .opcode=PS, .run=get_sp },
	{ .opcode=PH, .run=push },
	{ .opcode=PP, .run=pop },
	{ .opcode=CE, .run=cmp_eql },
	{ .opcode=CL, .run=cmp_less },
	{ .opcode=BT, .run=jmp_if },
	{ .opcode=BU, .run=jmp },
	{ .opcode=GD, .run=read },
	{ .opcode=PD, .run=print },
	{ .opcode=AD, .run=add },
	{ .opcode=SU, .run=subtract },
	{ .opcode=MI, .run=multiply },
	{ .opcode=DI, .run=divide },
	{ .opcode=AS, .run=add_stack },
	{ .opcode=SS, .run=subtract_stack },
	{ .opcode=MS, .run=multiply_stack },
	{ .opcode=DS, .run=divide_stack },
	{ .opcode=NP, .run=nop },
	{ .opcode=SD, .run=send },
	{ .opcode=RC, .run=recv },
	{ .opcode=GP, .run=identify },
	{ .opcode=HA, .run=halt }
};

struct proc proc_table[10];

int tick(unsigned int pid)
{
	char word[4];
	char temp[4];
	enum OP op;
	int addr, ic, i;
	struct proc *p;

	if (pid > 9) {
		fprintf(stderr, "tick: invalid PID %u\n", pid);
		return 1;
	}
	p = &proc_table[pid];

	/* load the word at p->ic */
	memset(temp, '0', 4);
	memcpy(temp+2, p->ic, 2);
	ic = word2int(temp);
	if (ic == -1) {
		fprintf(stderr, "tick: invalid IC\n");
		return -1;
	}
	if (load(p->pid, ic, word) == -1) {
		fprintf(stderr, "tick: load failed\n");
		return -1;
	}

	/* increment p->ic */
	ic++;
	int2word(ic, temp);
	memcpy(p->ic, temp+2, 2);

	/* execute the instruction */
	op = OPCODE(word[0], word[1]);
	memset(temp, '0', 4);
	memcpy(temp+2, word+2, 2);
	addr = word2int(temp);

	for (i=0; i<LEN(op_table); i++) {
		if (op_table[i].opcode == op)
			return op_table[i].run(p, addr);
	}
	fprintf(stderr, "tick: Illegal operation %c%c (pid %u)\n", word[0], word[1], pid);
	return -1;
}
