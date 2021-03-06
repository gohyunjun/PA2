/**********************************************************************
 * Copyright (c) 2020
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

 /**
  * Ready queue of the system
  */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process* current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable* ptbr;


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with RW_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with RW_READ only should not be accessed with
 *   RW_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
    int pd_index = vpn / NR_PTES_PER_PAGE;
    int pte_index = vpn % NR_PTES_PER_PAGE;

    if (ptbr->outer_ptes[pd_index] == NULL) ptbr->outer_ptes[pd_index] = malloc(sizeof(struct pte_directory));

    ptbr->outer_ptes[pd_index]->ptes[pte_index].valid = true;

    if (rw == 2 || rw == 3) {
        ptbr->outer_ptes[pd_index]->ptes[pte_index].writable = true;
    }
    else {
        ptbr->outer_ptes[pd_index]->ptes[pte_index].writable = false;
    }


    for (int i = 0;; i++) {
        if (!mapcounts[i]) {

            ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn = i;
            mapcounts[i]++;


            return i;
        }
    }

    return -1;
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, writable, pfn) is set @false or 0.
 *   Also, consider carefully for the case when a page is shared by two processes,
 *   and one process is to free the page.
 */
void free_page(unsigned int vpn)
{
    int pd_index = vpn / NR_PTES_PER_PAGE;
    int pte_index = vpn % NR_PTES_PER_PAGE;


    ptbr->outer_ptes[pd_index]->ptes[pte_index].valid = false;
    ptbr->outer_ptes[pd_index]->ptes[pte_index].writable = false;
    ptbr->outer_ptes[pd_index]->ptes[pte_index].private = 0;
    mapcounts[ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn]--;

}


/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid

 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
    int pd_index = vpn / NR_PTES_PER_PAGE;
    int pte_index = vpn % NR_PTES_PER_PAGE;

    if (!(ptbr->outer_ptes[pd_index]->ptes[pte_index].writable) && (rw == 2 || rw == 3) && ptbr->outer_ptes[pd_index]->ptes[pte_index].private == 1) {
        ptbr->outer_ptes[pd_index]->ptes[pte_index].writable = true;

        if (mapcounts[ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn] > 1) {

            mapcounts[ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn]--;


            for (int i = 0;; i++) {
                if (!mapcounts[i]) {

                    ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn = i;
                    mapcounts[i]++;
                    break;
                }
            }
        }

        return true;
    }


    return false;
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process;
 *   @current should be replaced to the requested process. Make sure that
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table.
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You can use pte->private to
 *   remember whether the PTE was originally writable or not.
 */
void switch_process(unsigned int pid)
{
    /** YOU CAN CHANGE EVERYTING (INCLUDING THE EXAMPLE) IN THIS FUNCTION **/

    struct process* p;

    bool find_flag = false;

    /* This example shows to iterate @processes to find a process with @pid */
    list_for_each_entry(p, &processes, list) {
        if (p->pid == pid) {
            /* FOUND */
            find_flag = true;

            break;
        }
    }

    if (!find_flag) {
        p = malloc(sizeof(*p));		/* This example shows to create a process, */
        INIT_LIST_HEAD(&p->list);	/* initialize list_head, */
        list_add_tail(&p->list, &processes);    /* and add it to the @processes list */
        p->pid = pid;

        for (int i = 0; i < NR_PAGEFRAMES / NR_PTES_PER_PAGE; i++) {
            for (int j = 0; j < NR_PTES_PER_PAGE; j++) {

                if (ptbr->outer_ptes[i] == NULL) break;
                if (p->pagetable.outer_ptes[i] == NULL) p->pagetable.outer_ptes[i] = malloc(sizeof(struct pte_directory));

                if (ptbr->outer_ptes[i]->ptes[j].writable || ptbr->outer_ptes[i]->ptes[j].private == 1) {
                    p->pagetable.outer_ptes[i]->ptes[j].private = 1;
                }

                p->pagetable.outer_ptes[i]->ptes[j].valid = ptbr->outer_ptes[i]->ptes[j].valid;
                p->pagetable.outer_ptes[i]->ptes[j].pfn = ptbr->outer_ptes[i]->ptes[j].pfn;

                if (ptbr->outer_ptes[i]->ptes[j].valid) mapcounts[ptbr->outer_ptes[i]->ptes[j].pfn]++;

                p->pagetable.outer_ptes[i]->ptes[j].writable = false;
                
            }
        }
        current = p;
        ptbr = &(current->pagetable);
    }
    else {
        for (int i = 0; i < NR_PAGEFRAMES / NR_PTES_PER_PAGE; i++) {
            for (int j = 0; j < NR_PTES_PER_PAGE; j++) {

                if (ptbr->outer_ptes[i] == NULL) break;
                if (p->pagetable.outer_ptes[i] == NULL) p->pagetable.outer_ptes[i] = malloc(sizeof(struct pte_directory));

                if (ptbr->outer_ptes[i]->ptes[j].writable || ptbr->outer_ptes[i]->ptes[j].private == 1) {
                    p->pagetable.outer_ptes[i]->ptes[j].private = 1;
                }

                if (!p->pagetable.outer_ptes[i]->ptes[j].valid) {

                    p->pagetable.outer_ptes[i]->ptes[j].valid = ptbr->outer_ptes[i]->ptes[j].valid;
                    p->pagetable.outer_ptes[i]->ptes[j].pfn = ptbr->outer_ptes[i]->ptes[j].pfn;

                }
                    p->pagetable.outer_ptes[i]->ptes[j].writable = false;

            }
        }
        current = p;
        ptbr = &(current->pagetable);

        find_flag = false;
    }

}

