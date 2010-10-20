struct plist
{
	struct proc *p;
	struct plist *next;
};

struct plist_head
{
	struct plist *head;
	struct plist *tail;
};

int insert_proc(struct plist_head *l, struct proc *p);
int remove_proc(struct plist_head *l, struct proc *p);
void free_list(struct plist_head *l);
