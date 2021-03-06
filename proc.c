#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

/***************************** LINKED LIST MANAGER **********/
//Head should be the process that is currently running, could just call myproc()
// to get the current running process
struct proc *head = 0;
struct proc *curr = 0;

int push(struct proc *proc) {
	if (head == 0) {
		head = proc;
		proc->next = 0;
		return 0;
	} else {
		curr = head;		
		while (curr->next != 0) {
      curr = curr->next;
    }
		curr->next = proc;
		proc->next = 0;
		return 0;
	}
	return -1;
}

struct proc* pop() {
	if (head == 0) {
		return 0;
	} else {
		struct proc *retProc = head;
		head = head->next;
		return retProc;
	}
	return 0;
}

int pushToBack(struct proc *proc) {
	curr = head;
	struct proc *prev = 0;
	if (proc == 0) {
		return -1;
	} else if (head == 0) {
		head = proc;
		proc->next = 0;
		return 1;
	} else {
    // Find the proc to push to back
		while (curr->pid != proc->pid) {
			if (curr->next != 0) {
				prev = curr;
				curr = curr->next;
        // If we get to the end of the linked list and it was not found
			} else {
				return -1;
			}
		}
		// at this point we've either returned -1 or curr is the node we want to push to back
		if (curr == head) {
			push(pop());
		} else {
			prev->next = curr->next;
			curr->next = 0;
			push(curr);
		}
	}
	return 1;
}
/*********************************************************/

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
	//push(p);
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

	// Push new process to the end of the scheduling queue
	// NOTE: This process may exist in the proc[NPROC] array
	// at an arbitrary location, however, push and pop manage
	// the order of the queue, which scheduler() will read from

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
	
	// this is the first process, there isn't any others
	
	// there isn't any other processes running so we null the next pointer
	//p->next = 0;
	// default timeslice of 1 tick
	p->timeslice = 1;
	p->totalComp = 0;
	p->givenComp = 0;
	p->totalTicks = 0;
	p->sleepTicks = 0;
	p->switches = 0;
	p->sleepDeadline = 0;
  p->runningTicks = 0;
	push(p);

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;


  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
	//np->next = 0;
	// Inherit the timeslice of the parent
	np->timeslice = curproc->timeslice;
	np->totalComp = 0;
	np->givenComp = 0;
	np->totalTicks = 0;
	np->sleepTicks = 0;
	np->switches = 0;
	np->sleepDeadline = 0;
  np->runningTicks = 0;
	push(np);

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->givenComp = 0;
  curproc->runningTicks = 0;
  pop();
	if (head != 0) {
		head->switches++;
    head->runningTicks = 0;
	}
	sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

		// !I dont think we need to acquire the ptable lock anymore
    acquire(&ptable.lock);

		// whenever scheduler gets invoked, the process now at the head should be scheduled
		if (head != 0) {
      acquire(&tickslock);
      //change this to use runningTicks
      if(head->runningTicks >= head->timeslice + head->givenComp){
        head->givenComp = 0;
        head->runningTicks = 0;
        push(pop());
				if (head != 0) {
        	head->switches++;
        	head->runningTicks = 0;
				}
      }
      p = head;
      p->totalTicks++;
      p->runningTicks++;
			//In the compticks
      if(p->runningTicks >= p->timeslice && p->runningTicks < p->timeslice + p->givenComp){
        p->totalComp++;
      }
      release(&tickslock);
			
      
			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;

			//p->switches++;
      /*p->totalTicks += p->timeslice + p->givenComp;
      p->totalComp += p->givenComp;*/
			swtch(&(c->scheduler), p->context);
			switchkvm();
		
			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;

		}
		release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  // Reset givenComp for the next cycle to avoid accumulation
  
  p->givenComp = 0;
  p->runningTicks = 0;
  pop();
  
	if (head != 0) {
		head->switches++;
    head->runningTicks = 0;
	}
  
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  //Changed this so that is doesn't wakeup all processes at once
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    // last check is to make sure the current time is past or equal to the
    // time that the process should be woken up
    if(p->state == SLEEPING && p->chan == chan && p->sleepDeadline <= ticks){
      p->state = RUNNABLE;
      // Now that its awake, push it to the end of the queue
      // was push to back
      push(p);
    }
    // Increment the compensation ticks while a process is sleeping
    else if(p->state == SLEEPING && p->chan == chan){
      p->givenComp++;
      p->sleepTicks++;
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      // push to end because waking it up
      //TODO: Maybe adhere to sleepdeadline, probably not tho
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        p->runningTicks = 0;
        push(p);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

/***************************************
**       User Added Functions         **
***************************************/

/*
* Sets the time slice of the specified pid to slice
* Returns -1 on failure and 0 on success
*/
int setslice(int pid, int slice) {
	if(pid <= 0 || slice <= 0){
    return -1;
  } else {
    struct proc *p;
		// snag the ptable lock | can probably move this later to make more efficient
		acquire(&ptable.lock);
    //Maybe just go through pstast? I just copied this from kill which takes a pid
    // p is a pointer and p++ increments the pointer
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->pid == pid){
        p->timeslice = slice;
        release(&ptable.lock);
        return 0;
      }
    }
		release(&ptable.lock);
	}
	// pid was not found if we reach here
  return -1;
}

/*
* Returns the time slice of the process with the pid given
* Returns -1 if the pid is not valid
*/
int getslice(int pid) {
	if (pid <= 0) {
		return -1;
	} else {
    struct proc *p;
    acquire(&ptable.lock);
    //p = same as doing = &ptable.proc[0]
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->pid == pid){
				int timeslice = p->timeslice;
        release(&ptable.lock);
        return timeslice;
      }
    }
    release(&ptable.lock);
	}
  // pid was not found if we reach here
  return -1;
}

/*
* Similar to fork except now the newly created process
* gets the specified time-slice 
* Returns -1 if the slice given is not positive
*/
int fork2(int slice) {
  if(slice <= 0){
    return -1;
  }
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  //np->next = 0;
  // Set the timeslice equal to the number that was passed in
  np->timeslice = slice;
  np->totalComp = 0;
  np->givenComp = 0;
  np->totalTicks = 0;
  np->sleepTicks = 0;
  np->switches = 0;
	np->sleepDeadline = 0;
  np->runningTicks = 0;
	push(np);

  release(&ptable.lock);

  return pid;
} 

/*
* Extract useful information about pstat
* Return 0 on success, -1 on failure
*/
int getpinfo(struct pstat *pst) {
  int i;
  if(pst == 0){
    return -1;
  }
  // Populate the pstat
  for(i = 0; i < NPROC; i++){
    struct proc *p = &ptable.proc[i];
    if(p->state == UNUSED) {
      pst->inuse[i] = 0;
    } else{
      pst->inuse[i] = 1;
    }
    pst->pid[i] = p->pid;
    pst->timeslice[i] = p->timeslice;
    pst->compticks[i] = p->totalComp;
    pst->schedticks[i] = p->totalTicks;
    pst->sleepticks[i] = p->sleepTicks;
    pst->switches[i] = p->switches;
  }
  return 0;
}
