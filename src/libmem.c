/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /* Allocate at the top of the free region */
  struct vm_rg_struct rgnode;

  /* Attempt to find a free virtual memory region */
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    /* Update the symbol table with the allocated region */
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    /* Return the starting address of the allocated region */
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* Handle the case where no free region is found */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL)
    return -1; // Invalid VMA

  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int inc_limit_ret = inc_vma_limit(caller, vmaid, inc_sz);
  if (inc_limit_ret < 0)
    return -1; // Failed to increase VMA limit

  /* Retry finding a free region after increasing the limit */
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    /* Update the symbol table with the allocated region */
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    /* Return the starting address of the allocated region */
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* If all attempts fail, return an error */
  pthread_mutex_unlock(&mmvm_lock);
  return -1;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  // Validate the region ID
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  // Retrieve the memory region associated with the region ID
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);
  if (rgnode == NULL)
    return -1; // Invalid region ID

  // Add the freed region back to the free region list
  if (enlist_vm_freerg_list(caller->mm, rgnode) < 0)
    return -1; // Failed to add the region to the free list

  // Clear the symbol table entry for the region
  caller->mm->symrgtbl[rgid].rg_start = -1;
  caller->mm->symrgtbl[rgid].rg_end = -1;

  return 0; // Success
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 0 */
  int result = __alloc(proc, 0, reg_index, size, &addr);

  if (result == 0)
  {
#ifdef IODUMP
    printf("Allocated memory: Region=%d, Size=%u, Address=%d\n", reg_index, size, addr);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // Print the page table for debugging
#endif
    MEMPHY_dump(proc->mram); // Dump the memory state
#endif
  }
  else
  {
#ifdef IODUMP
    printf("Failed to allocate memory: Region=%d, Size=%u\n", reg_index, size);
#endif
  }

  return result;
}

// TEST:
// int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
// {
//     int addr;

//     /* By default using vmaid = 0 */
//     return __alloc(proc, 0, reg_index, size, &addr);
// }

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

 int libfree(struct pcb_t *proc, uint32_t reg_index)
 {
   /* Validate the region index */
   if (reg_index >= PAGING_MAX_SYMTBL_SZ)
     return -1; // Invalid region index
 
   /* By default using vmaid = 0 */
   int result = __free(proc, 0, reg_index);
 
 #ifdef IODUMP
   if (result == 0)
   {
     printf("Freed memory: Region=%d\n", reg_index);
 #ifdef PAGETBL_DUMP
     print_pgtbl(proc, 0, -1); // Print the page table for debugging
 #endif
     MEMPHY_dump(proc->mram); // Dump the memory state
   }
   else
   {
     printf("Failed to free memory: Region=%d\n", reg_index);
   }
 #endif
 
   return result;
 }

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    int vicpgn, swpfpn, vicfpn;
    uint32_t vicpte;

    /* Find victim page */
    find_victim_page(caller->mm, &vicpgn);

    /* Get the victim frame number */
    vicpte = mm->pgd[vicpgn];
    vicfpn = PAGING_PTE_FPN(vicpte);

    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
      return -1; // No free frame in swap space

    /* Swap victim frame to MEMSWP */
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);

    /* Update victim page table entry to mark it as swapped */
    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

    /* Bring the target page from MEMSWP to MEMRAM */
    int tgtfpn = PAGING_PTE_SWP(pte);
    __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);

    /* Update target page table entry to mark it as present */
    pte_set_fpn(&mm->pgd[pgn], vicfpn);

    /* Enlist the target page in the FIFO page list */
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* Invalid page access */

  /* Calculate the physical address */
  int phyaddr = (fpn << NBITS(PAGING_PAGESZ)) | off;

  /* Read the value from physical memory */
  if (MEMPHY_read(caller->mram, phyaddr, data) != 0)
    return -1; /* Failed to read from memory */

  return 0; // Success
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* Invalid page access */

  /* Calculate the physical address */
  int phyaddr = (fpn << NBITS(PAGING_PAGESZ)) | off;

  /* Write the value to physical memory */
  if (MEMPHY_write(caller->mram, phyaddr, value) != 0)
  return -1; /* Failed to write to memory */

  return 0; // Success
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  //destination 
  *destination = data; // VERY IMPORTANT

#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  if (pg == NULL)
    return -1; // No pages to evict

  /* Use FIFO to find the victim page */
  *retpgn = pg->pgn; // Get the page number of the first node
  mm->fifo_pgn = pg->pg_next; // Remove the victim from the FIFO list

  free(pg); // Free the memory of the victim node

  return 0; // Success
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1; // No free regions available

  /* Traverse the free region list to find a suitable space */
  while (rgit != NULL)
  {
    if ((rgit->rg_end - rgit->rg_start) >= size)
    {
      /* Found a suitable region */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update the free region list */
      rgit->rg_start += size;
      if (rgit->rg_start == rgit->rg_end)
      {
        /* Remove the region if fully used */
        cur_vma->vm_freerg_list = rgit->rg_next;
        free(rgit);
      }

      return 0; // Success
    }

    rgit = rgit->rg_next;
  }

  return -1; // No suitable region found
}

//#endif
