/*
*********************************************************************************************************
*                                               uC/OS-II
*                                         The Real-Time Kernel
*
*                              OS Practice Final Project -- RM Scheduler
*
* Description: Reads taskset.txt, creates periodic tasks, and schedules them
*              using Rate Monotonic (RM): shorter period = higher priority.
*              Priority assignment is static and done at task creation time.
*
* TODO (Teammate A): Implement TaskStartCreateTasks() and PeriodicTask().
*   See WORKPLAN.md Section III for details and hints.
*********************************************************************************************************
*/

#include "includes.h"

/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define  TASK_STK_SIZE   512
#define  MAX_TASKS         7

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

OS_STK  TaskStk[MAX_TASKS][TASK_STK_SIZE];
OS_STK  TaskStartStk[TASK_STK_SIZE];

INT32U  TaskPeriod[MAX_TASKS];
INT32U  TaskExecTime[MAX_TASKS];
int     TaskCount = 0;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void  TaskStart(void *pdata);
void  PeriodicTask(void *pdata);
static void  TaskStartCreateTasks(void);
static void  TaskStartDispInit(void);

/*
*********************************************************************************************************
*                                                MAIN
*********************************************************************************************************
*/

void  main (void)
{
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);

    OSInit();

    PC_DOSSaveReturn();
    PC_VectSet(uCOS, OSCtxSw);

    OSTaskCreate(TaskStart, (void *)0, &TaskStartStk[TASK_STK_SIZE - 1], 0);
    OSStart();
}

/*
*********************************************************************************************************
*                                           STARTUP TASK
*********************************************************************************************************
*/

void  TaskStart (void *pdata)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr;
#endif
    INT16S  key;

    pdata = pdata;

    TaskStartDispInit();

    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();

    OSStatInit();
    TaskStartCreateTasks();

    for (;;) {
        if (PC_GetKey(&key) == TRUE) {
            if (key == 0x1B) {
                PC_DOSReturn();
            }
        }
        OSTimeDlyHMSM(0, 0, 1, 0);
    }
}

/*
*********************************************************************************************************
*                                       READ TASKSET AND CREATE TASKS
*
* RM rule: shorter period -> higher priority (smaller priority number).
*
* Steps:
*   1. Open "taskset.txt", read TaskCount, then read (exec_time, period) for each task.
*   2. Sort tasks by period ascending -- shortest period gets priority 1.
*   3. Call OSTaskCreateExt() for each task in sorted order, assigning priority 1, 2, 3...
*      Pass TaskPeriod[i] and TaskExecTime[i] as the last two arguments (period, exec_time).
*********************************************************************************************************
*/

static  void  TaskStartCreateTasks (void)
{
    FILE   *fp;
    int     i, j;
    INT32U  tmpP, tmpE;
    INT8U   prio;
    char    s[80];

    fp = fopen("taskset.txt", "r");
    if (fp == (FILE *)0) {
        PC_DispStr(0, 5, "ERROR: cannot open taskset.txt", DISP_FGND_RED + DISP_BGND_BLACK);
        return;
    }

    fscanf(fp, "%d", &TaskCount);
    for (i = 0; i < TaskCount; i++) {
        fscanf(fp, "%ld %ld", &TaskExecTime[i], &TaskPeriod[i]);
    }
    fclose(fp);

    /* TODO: Sort TaskPeriod[] and TaskExecTime[] together by period ascending */
    /*       Hint: bubble sort -- swap both arrays together when TaskPeriod[j] > TaskPeriod[j+1] */

    /* TODO: For each task i, assign prio = i+1, then call OSTaskCreateExt() */
    /*       Pass PeriodicTask as the task function, (void*)(i+1) as pdata    */
    /*       Pass TaskPeriod[i] and TaskExecTime[i] as the last two arguments */
    /*       Display each task's info with PC_DispStr at row 5+i              */
}

/*
*********************************************************************************************************
*                                           PERIODIC TASK BODY
*
* Each task should:
*   1. Record the current tick as start_tick (use OSTimeGet()).
*   2. Busy-wait until (OSTimeGet() - start_tick) >= OSTCBCur->OSTCBExecTime.
*      This simulates computation and can be preempted by a higher-priority task.
*   3. Display which task ran and at what tick.
*   4. Calculate remaining time in the period: delay = OSTCBCur->OSTCBPeriod - elapsed.
*   5. Call OSTimeDly(delay) to sleep until the next period begins.
*
* Access the current task's TCB fields via OSTCBCur->OSTCBExecTime, OSTCBCur->OSTCBPeriod.
* task_id is passed via pdata.
*********************************************************************************************************
*/

void  PeriodicTask (void *pdata)
{
    INT32U  start_tick;
    INT32U  elapsed;
    INT32U  delay_ticks;
    INT8U   task_id;
    char    s[80];
    int     row;

    task_id = (INT8U)(INT32U)pdata;
    row     = 13 + (int)task_id;

    for (;;) {
        /* TODO: start_tick = OSTimeGet(); */

        /* TODO: busy-wait loop for OSTCBCur->OSTCBExecTime ticks */

        /* TODO: elapsed = OSTimeGet() - start_tick; */

        /* TODO: display task execution info using PC_DispStr at row */

        /* TODO: delay_ticks = OSTCBCur->OSTCBPeriod - elapsed; */
        /* TODO: if delay_ticks > 0, call OSTimeDly((INT16U)delay_ticks); */
    }
}

/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDispInit (void)
{
    PC_DispStr(0,  0, "         uC/OS-II  --  RM Scheduler  --  OS Practice Final Project         ",
               DISP_FGND_WHITE + DISP_BGND_RED);
    PC_DispStr(0,  2, "Scheduler: Rate Monotonic (RM)  |  shorter period = higher priority",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
