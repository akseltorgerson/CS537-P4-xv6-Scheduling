//Copyright 2021 Alec Keehbler Aksel Torgerson
//#include "proc.h"

#define NULL ((char*)0)

struct listNode_t {
	struct proc *proc;
  struct listNode_t *next; 
};

/* Add process to the tail of the queue */
int queueProc(struct proc *proc);

/* Pop the process of the front of the queue */
struct proc* popProc();

/* Peek the process at the front of the queue */
struct proc* peekProc(struct proc *proc);


