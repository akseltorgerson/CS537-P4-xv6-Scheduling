// Copyright 2021 Alec Keehbler Aksel Torgerson
#include "linkedlist.h"

/*
* Global Variables for tracking the head, tail, and the current 
* node that is being used
*/
struct listNode_t *head = NULL;
struct listNode_t *curr = NULL;

/* Add to the tail of the queue */
int queueProc(struct proc *proc) {
	/* Allocate heap memory and init a new node */
	struct listNode_t* newNode;
	newNode = (struct listNode_t*)malloc(sizeof(struct listNode_t*));
	
	/* Check malloc return value */
	if (newNode == NULL) {
		return -1;
	}

	/* Setup the newNode */
	newNode->proc = proc;
	newNode->next = NULL;

	/* If there are no processes in the queue so far */
	if (head == NULL) {
		head = newNode;
	} else {
		curr = head;
		while (curr->next != NULL) {curr = curr->next;}
		curr->next = newNode;
	}
	return 0;
}

/* Pop the process from the front of the list */
struct proc* popProc() {
	if (head != NULL) {
		struct proc *retProc = head->proc;
		head = head->next;
		return retProc;
	} else {
		return NULL;
	}
}

/* Peeks from the front of the queue */
struct proc* peekProc() {
	if (head != NULL) {
		struct proc *retProc = head->proc;
		return retProc;
	} else {
		return NULL;
	}
}
