#define LEN(a) (sizeof(a)/sizeof((a)[0]))

struct proc
{
	char r[4];
	char sp[4];
	char br[4];
	char lr[4];
	char ic[2];
	char c;
	unsigned int stack_base;
	unsigned int pid;
};

extern struct proc proc_table[10];

struct plist
{
	struct proc *p;
	struct plist *next;
};

#define OPCODE(a, b) (((int)(a)<<8)+(b))

enum OP {
	LR = OPCODE('L', 'R'),
	LL = OPCODE('L', 'L'),
	LH = OPCODE('L', 'H'),
	SR = OPCODE('S', 'R'),
	SP = OPCODE('S', 'P'),
	PS = OPCODE('P', 'S'),
	PH = OPCODE('P', 'H'),
	PP = OPCODE('P', 'P'),
	CE = OPCODE('C', 'E'),
	CL = OPCODE('C', 'L'),
	BT = OPCODE('B', 'T'),
	BU = OPCODE('B', 'U'),
	GD = OPCODE('G', 'D'),
	PD = OPCODE('P','D'),
	AD = OPCODE('A', 'D'),
	SU = OPCODE('S', 'U'),
	MI = OPCODE('M', 'U'),
	DI = OPCODE('D', 'I'),
	AS = OPCODE('A', 'S'),
	SS = OPCODE('S', 'S'),
	MS = OPCODE('M', 'S'),
	DS = OPCODE('D', 'S'),
	NP = OPCODE('N', 'P'),
	HA = OPCODE('H', 'A')
};

struct op
{
	enum OP opcode;
	int (*run)(struct proc *p, int addr);
};

int tick(unsigned int pid);
	
