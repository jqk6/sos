#include <system_config.h>
#include <libc.h>
#include <os.h>
#include "log.h"

PRIVATE void task_sched(struct __os_task__ *best_task);

struct __os_task__ tcb[TASK_NR_MAX]        __attribute__((__aligned__(0x100)));
u32 task_stack[TASK_NR_MAX][TASK_STK_SIZE] __attribute__((__aligned__(0x100)));

/* get current task id, little hack */
PRIVATE u32 get_current_task()
{
    u32 task_id;
    u32 sp = __get_sp();

    task_id = (sp - (u32)task_stack) / (TASK_STK_SIZE * 4);
    PRINT_EMG("task_id: %d\n", task_id);
    kassert(task_id <= TASK_NR_MAX);
    return task_id;
}

/* get the highest priority task in os_ready_list */
PUBLIC struct __os_task__ * get_task_ready()
{
    struct __os_task__ *ptask;
    ptask = os_ready_list.next;
    PRINT_DEBUG("get_task_ready %x \n", ptask);
    return ptask;
}

PUBLIC struct __os_task__ * tcb_alloc()
{
    u32 i;

    for(i = 0; i < sizeof(tcb)/sizeof(tcb[0]); i++) {
        if (tcb[i].state == TASK_UNUSED) {
            tcb[i].id = i;
            return &tcb[i];
        }
    }
    return NULL;
}

PRIVATE void task_matrix(u32 addr, u32 arg)
{
    u32 current_task_id;
    func_1 task_entry = (func_1)addr;
    task_entry(arg);
    current_task_id = get_current_task();

    task_delete(current_task_id);
    while(1);
}

/* PRIVATE */ s32 tcb_init(struct __os_task__ *ptask, func_1 task_entry, u32 arg, u32 priority)
{
    struct __cpu_context__ *cc;

    ptask->state = TASK_READY;
    ptask->prio = priority;
    ptask->stack = &task_stack[ptask->id][0];
    ptask->stack_size = TASK_STK_SIZE;
    ptask->entry = task_entry;
    ptask->sleep_ticks = 0;
    ptask->events = 0;

    /* task context init */
    cc = (struct __cpu_context__ *)
        (&(ptask->stack[TASK_STK_SIZE - (sizeof(struct __cpu_context__) / 4)]));

    cc->cpsr = 0x15F;   /* irq enable, fiq disable, arm instruction, system mode */
    cc->r0   = (u32)task_entry;
    cc->r1   = arg;
    cc->r2   = 0;
    cc->r3   = 0;
    cc->r4   = 0;
    cc->r5   = 0;
    cc->r6   = 0;
    cc->r7   = 0;
    cc->r8   = 0;
    cc->r9   = 0;
    cc->r10  = 0;
    cc->r11  = 0;
    cc->r12  = 0;
    cc->sp   = (u32)(&ptask->stack[TASK_STK_SIZE]);
    cc->lr   = 0;
    cc->pc   = (u32)task_matrix + 4;     /* real pc + 4, create the fake irq mode saved cpu_context */

    ptask->sp = (u32)cc;

    ptask->stack[0] = 0xbadbeef;

    return 0;
}

PUBLIC struct __os_task__ * get_best_task()
{
    struct __os_task__ *best_task = NULL;

    best_task = get_task_ready(); /* get the highest priority task in READY STATE */

    PRINT_DEBUG("get best_task %x \n", best_task);
#if 0
    if (best_task->prio <= current_task->prio || /* current_task create a higher prio task  */
        current_task->state == TASK_UNUSED    || /* current_task self-destruction  */
        current_task->state == TASK_SLEEP     || /* current_task invoke os_sleep */
        current_task->state == TASK_WAIT_SEM     /* current_task wait for semaphore */
        ) {
        PRINT_DEBUG("schedule task %d \n", best_task->id);
        return best_task;
    }
#endif
    return best_task;
}

PUBLIC s32 task_dispatch()
{
    struct __os_task__ *best_task;

    best_task = get_best_task();
    PRINT_DEBUG("task_dispatch %d \n", best_task->id);
    task_sched(best_task);

    return 0;
}

PUBLIC s32 task_create(func_1 entry, u32 arg, u32 prio)
{
    struct __os_task__ *ptask;
    if ((ptask = tcb_alloc()) == NULL) {
        kassert(0);
    }

    tcb_init(ptask, entry, arg, prio);

    os_ready_insert(ptask); /* FIXME: do task switch immediately if a higher task created. */

    if (ptask->prio < current_task->prio) {
        current_task->state = TASK_READY;
        task_dispatch();
    }

    return ptask->id;
}

PUBLIC s32 task_delete(u32 task_id)
{
    tcb[task_id].prio  = TASK_PRIO_MAX; /* lowest prio */
    tcb[task_id].state = TASK_UNUSED;
    /* FIXME: do task switch immediately */
    return 0;
}

PUBLIC s32 task_sleep(u32 ticks)
{
    current_task->sleep_ticks = ticks;
    current_task->state       = TASK_SLEEP;
    task_dispatch();
    return 0;
}

/*
   1. update new_task   (request a task switch)
   2. delete the best_task from os_ready_list
   3. insert old_task into os_sleep_list or os_ready_list or sem_list or mbx_list.
*/
PRIVATE void task_sched(struct __os_task__ *best_task)
{
    struct __os_task__ *old_task;
    struct __os_semaphore__ *psem;
    struct __os_mailbox__ *pmbx;

    old_task = current_task;
    new_task = best_task;

    new_task->state = TASK_RUNNING;
    os_ready_delete(best_task);

    switch (old_task->state) {
        case (TASK_UNUSED):     /* current task self-destruction */
            break;
#if 0
        case (TASK_RUNNING):    /* current task create higher prio task  */
            old_task->state = TASK_READY;
            os_ready_insert(old_task);
            break;
#endif
        case (TASK_READY):      /* current task create higher prio task  */
            os_ready_insert(old_task);
            break;
        case (TASK_SLEEP):      /* current task invoke os_task_sleep sleep */
            os_sleep_insert(old_task);
            break;
        case (TASK_WAIT_SEM):
            psem = (struct __os_semaphore__ *)(old_task->private_data);
            os_sem_insert(psem, old_task);
            break;
        case (TASK_WAIT_MBX):
            pmbx = (struct __os_mailbox__ *)(old_task->private_data);
            os_mbx_insert(pmbx, old_task);
            break;
        case (TASK_WAIT_EVENT):
            /* TODO */
            break;
        default:
            kassert(0);
    }

    /* dump_list(); */
    PRINT_DEBUG("schedule %d \n", new_task->id);
}

PUBLIC s32 task_init()
{
    return 0;
}
