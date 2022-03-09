#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;
    char *frame;
    int refcnt;
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

int shm_open(int id, char **pointer) {
  int i;
  int foundID = 0;
  acquire(&(shm_table.lock));
  uint currSz = PGROUNDUP(myproc()->sz);
  //Look through shm_table and see if the id we are 
  //opening already exists
  for (i = 0; i < 64; i++) {
    //Case 1
    //Already exists so another process did a shm_open
    //before us. Find physical address of the page in the table
    //and map it to an available page in our virtual address space
    if (shm_table.shm_pages[i].id == id) {
      struct proc *curproc = myproc();
      mappages(curproc->pgdir, (char *)currSz, PGSIZE, V2P(shm_table.shm_pages[i].frame), PTE_W|PTE_U);
      shm_table.shm_pages[i].refcnt++;
      *pointer = (char *)currSz; 
      foundID = 1;
      break;
    }
  }

  //Case 2
  //Shared memory segment does not exist
  //First processt to do shm_open
  if (foundID == 0) {
    for (i = 0; i < 64; i++) {
      //Find an empty entry in the shm_table
      if ( (shm_table.shm_pages[i].id == 0) && (shm_table.shm_pages[i].frame == 0) && (shm_table.shm_pages[i].refcnt == 0) ) {
        //Initialize its id to the id passed to us
        shm_table.shm_pages[i].id = id;
        //kmalloc a page and store its address in frame
        shm_table.shm_pages[i].frame = kalloc();
        //and store its address in frame
        memset(shm_table.shm_pages[i].frame, 0, PGSIZE);
        //Set the refcnt to 1
        shm_table.shm_pages[i].refcnt = 1;
        //map the page to an available virtual address
        //space page (e.g., sz), and return a pointer through the pointer
        //parameter.
        mappages(myproc()->pgdir, (char*)currSz, PGSIZE, V2P(shm_table.shm_pages[i].frame), PTE_W|PTE_U);
        *pointer = (char *)currSz; 
        break;
      }
    }
  }
  release(&(shm_table.lock));
  return 0;
}


int shm_close(int id) {
  //it looks for the shared memory segment in shm_table. 
  //If it finds it it decrements the reference count. 
  //If it reaches zero, then it clears the shm_table
  acquire(&(shm_table.lock));
  int i;
  for (i = 0; i < 64; i++) {
    if (shm_table.shm_pages[i].id == id) {
      shm_table.shm_pages[i].refcnt--;
      if (shm_table.shm_pages[i].refcnt == 0) {
        shm_table.shm_pages[i].id = 0;
        shm_table.shm_pages[i].frame = 0;
      }
    }
  }
  release(&(shm_table.lock));
  return 0; 
}
