
#include "queue.h"
#include "sched.h"
#include "common.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
static int current =0;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
    unsigned long prio;
    for (prio = 0; prio < MAX_PRIO; prio++)
        if (!empty(&mlq_ready_queue[prio])) 
            return -1;
#endif
    return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i;
    for (i = 0; i < MAX_PRIO; i++) {
        mlq_ready_queue[i].size = 0;
        mlq_ready_queue[i].time_slot = MAX_PRIO - i; 
    }
#endif
    ready_queue.size = 0;
    run_queue.size = 0;
    running_list.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
struct pcb_t * get_mlq_proc(void) {
    struct pcb_t * proc = NULL;
    pthread_mutex_lock(&queue_lock);
   
    int empty_queue = 0;
    int all_slots_empty = 1; // Kiểm tra xem tất cả time slot có về 0 không

    for (int i = 0; i < MAX_PRIO; i++) {
        if (!empty(&mlq_ready_queue[i])) {
            if (mlq_ready_queue[i].time_slot > 0) {
                proc = dequeue(&mlq_ready_queue[i]);
                mlq_ready_queue[i].time_slot--;
                all_slots_empty = 0; // Có ít nhất một queue còn time slot
                break;
            }
        } else {
            empty_queue++;
        }
    }

    // Nếu tất cả time slot về 0, reset lại cho từng hàng đợi
    if (all_slots_empty) {
        for (int j = 0; j < MAX_PRIO; j++) {
            mlq_ready_queue[j].time_slot = MAX_PRIO - j;
        }
    }

    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_mlq_proc(struct pcb_t * proc) {
    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

struct pcb_t * get_proc(void) {
    return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
    /*proc->ready_queue = &ready_queue;
    *proc->mlq_ready_queue = mlq_ready_queue;
    *proc->running_list = &running_list;*/
    
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);

    put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
    /*proc->ready_queue = &ready_queue;
    *proc->mlq_ready_queue = mlq_ready_queue;
    *proc->running_list = &running_list;*/
    
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);

    add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
    struct pcb_t * proc = NULL;
    pthread_mutex_lock(&queue_lock);
    if (!empty(&ready_queue)) {
        proc = dequeue(&ready_queue);
    }
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_proc(struct pcb_t * proc) {
    /*proc->ready_queue = &ready_queue;
    *proc->running_list = &running_list;*/
    
    pthread_mutex_lock(&queue_lock);
    enqueue(&ready_queue, proc);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
    /*proc->ready_queue = &ready_queue;
    *proc->running_list = &running_list;*/
    
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    enqueue(&ready_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}
#endif
