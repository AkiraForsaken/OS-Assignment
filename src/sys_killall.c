/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include <string.h>

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;

    //hardcode for demo only
    uint32_t memrg = regs->a1;
    
    /* TODO: Get name of the target proc */ 
    //proc_name = libread..
    int i = 0;
    data = 0;
    while(data != -1){
        libread(caller, memrg, i, &data);
        proc_name[i]= data;
        if(data == -1) proc_name[i]='\0';
        i++;
    }
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
    
    /* TODO: Traverse proclist to terminate the proc
     *       strcmp to check the process match proc_name
     */
    //caller->running_list
    //caller->mlq_ready_queue

    /* TODO Maching and terminating 
     *       all processes with given
     *        name in var proc_name
     */

    if (caller->running_list != NULL) {
        struct queue_t *running_list = caller->running_list;
        struct queue_t temp_queue = {.size = 0};  
        
        while (!empty(running_list)) { // Extract all processes from running_list
            struct pcb_t *proc = dequeue(running_list); // dequeue: terminate process
            
            if (strcmp(proc->path, proc_name) != 0) { // name doesn't match: keep process
                enqueue(&temp_queue, proc);
            }
        }

        while (!empty(&temp_queue)) { // Re-add the processes into the queue
            enqueue(running_list, dequeue(&temp_queue));
        }
    }

    /* if (caller->ready_queue != NULL) {
        struct queue_t *ready_queue = caller->ready_queue;
        struct queue_t temp_queue = {.size = 0};  
        
        while (!empty(ready_queue)) { 
            struct pcb_t *proc = dequeue(ready_queue); 
            
            if (strcmp(proc->path, proc_name) != 0) { 
                enqueue(&temp_queue, proc);
            }
        }

        while (!empty(&temp_queue)) { 
            enqueue(ready_queue, dequeue(&temp_queue));
        }
    } */ // Not sure if needed

    #ifdef MLQ_SCHED
        for (int i = 0; i < MAX_PRIO; i++){
            if (caller->mlq_ready_queue != NULL){
                struct queue_t *priority_queue = &caller->mlq_ready_queue[i];
                struct queue_t temp_queue = { .size = 0};
                while (!empty(priority_queue)){
                    struct pcb_t *proc = dequeue(priority_queue);
                    
                    if (strcmp(proc->path, proc_name) != 0) { 
                        enqueue(&temp_queue, proc);
                    }
                }

                while (!empty(&temp_queue)) { 
                    enqueue(priority_queue, dequeue(&temp_queue));
                }
            }
        }
        
    #endif

    return 0; 
}
