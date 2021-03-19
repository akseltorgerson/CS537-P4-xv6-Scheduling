#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "pstat.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  struct proc *curr = myproc();
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  // Deadline where a process should be awoken
  //curr->sleepDeadline = ticks0 + n;
  curr->sleepDeadline = ticks0 + n;
  //while(ticks - ticks0 < n){
  if(curr->killed){
    release(&tickslock);
    return -1;
  }
  sleep(&ticks, &tickslock);
  //}
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/*****************************************************
**                Added sys calls                   **
*****************************************************/

int sys_setslice(void)
{
  int pid, slice;
  //TODO: Not sure if this is the correct method for getting
  //two arugments
  if(argint(0, &pid) < 0){
    return -1;
  }
  if(argint(1, &slice) < 0){
    return -1;
  }
  return setslice(pid, slice);
}

int sys_getslice(void)
{
  int pid;
  // Copied this from kill 
  if(argint(0, &pid) < 0){
    return -1;
  }
  return getslice(pid);
}

int sys_fork2(void)
{
  int slice;
  // Copied this from kill
  if(argint(0, &slice) < 0){
    return -1;
  }
  return fork2(slice);
}

int sys_getpinfo(void)
{

  struct pstat *pstat1;
  // Got this from the piazza post FAQ which linked to a stackoverflow
  // maybe have sizeof(struct pstat)
  if(argptr(0, (void*)&pstat1, sizeof(*pstat1)) < 0){
    return -1;
  }

  return getpinfo(pstat1);
}


