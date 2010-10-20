#include <stdlib.h>
#include "wait_queue.h"

int insert_proc(struct plist_head *l, struct proc *p)
{
	struct plist *newp;

	if (l == NULL)
		return 1;
	if (p == NULL)
		return 1;

	newp = malloc(sizeof(struct plist));
	if (newp == NULL)
		return 1;
	newp->p = p;
	newp->next = NULL;
	if (l->head == NULL) {
		l->head = l->tail = newp;
	} else {
		l->tail->next = newp;
		l->tail = newp;
	}
	return 0;
}

int remove_proc(struct plist_head *l, struct proc *p)
{
	struct plist *listp, *elp;
	struct plist root;

	if (l == NULL)
		return 1;
	if (p == NULL)
		return 1;

	root.next = l->head;
	root.p = NULL;
	for (listp=&root; listp->next != NULL && listp->next->p != p; listp = listp->next);
	if (listp->next == NULL)
		return 0; /* proc not in list */
	elp = listp->next;
	listp->next = listp->next->next;
	if (listp->next == NULL)
		l->tail = listp;
	if (listp == &root)
		l->head = listp->next;
	free(elp);
	return 0;
}

void free_list(struct plist_head *l)
{
	struct plist *curp, *nextp;

	for (curp = l->head; curp != NULL; curp = nextp) {
		nextp = curp->next;
		free(curp);
	}
}
