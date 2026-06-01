/*
*********************************************************************************************************
*                                               uC/OS-II
*                                         The Real-Time Kernel
*
*                             OS Practice Final Project -- EDF Scheduler
*
* Description: Reads taskset.txt, creates periodic tasks, and schedules them
*              using Earliest Deadline First (EDF).
*              OS_Sched() (modified via #define SCHED_EDF in OS_CFG.H) dynamically
*              assigns priority 1 to the ready task with the earliest absolute deadline.
*              Each task must update its own OSTCBDeadline before sleeping.
*
* TODO (Teammate B): Implement TaskStartCreateTasks() and PeriodicTask().
*   See WORKPLAN.md Section IV for details and hints.
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
#define  BASE_PRIO        10    /* App tasks start at this priority; EDF will give prio 1 dynamically */

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
* EDF does NOT need tasks sorted by period at creation time.
* Assign priorities BASE_PRIO, BASE_PRIO+1, BASE_PRIO+2, ... (just stable slots).
* OS_Sched() will dynamically promote the earliest-deadline ready task to priority 1.
*
* Steps:
*   1. Open "taskset.txt", read TaskCount, then read (exec_time, period) for each task.
*   2. Call OSTaskCreateExt() for each task, assigning priority BASE_PRIO+i.
*      Pass TaskPeriod[i] and TaskExecTime[i] as the last two arguments (period, exec_time).
*   3. No sorting needed.
*********************************************************************************************************
*/

static  void  TaskStartCreateTasks (void)
{
    FILE   *fp;
    int     i;
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

    /* TODO: For each task i, assign prio = BASE_PRIO + i, then call OSTaskCreateExt() */
    /*       Pass PeriodicTask as task function, (void*)(i+1) as pdata                 */
    /*       Pass TaskPeriod[i] and TaskExecTime[i] as the last two arguments           */
    /*       Display each task's info with PC_DispStr at row 5+i                        */
    /*       No sorting -- EDF scheduler dynamically picks the earliest deadline task   */
}

/*
*********************************************************************************************************
*                                           PERIODIC TASK BODY
*
* Each task should:
*   1. Record the current tick as start_tick (use OSTimeGet()).
*   2. Busy-wait until (OSTimeGet() - start_tick) >= OSTCBCur->OSTCBExecTime.
*   3. Display which task ran and its current deadline.
*   4. IMPORTANT: Update OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod
*      This MUST happen BEFORE OSTimeDly() -- the scheduler reads this value on next wakeup.
*   5. Calculate remaining time in the period and call OSTimeDly().
*
* The deadline update in step 4 is what makes EDF work correctly across periods.
* Access the current task's TCB fields via OSTCBCur->OSTCBDeadline, OSTCBCur->OSTCBPeriod, etc.
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

        /* TODO: display task execution info (include OSTCBDeadline in output) */

        /* TODO: OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;  <-- MUST be before OSTimeDly */

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
    PC_DispStr(0,  0, "        uC/OS-II  --  EDF Scheduler  --  OS Practice Final Project          ",
               DISP_FGND_WHITE + DISP_BGND_RED);
    PC_DispStr(0,  2, "Scheduler: Earliest Deadline First (EDF)  |  earliest deadline gets prio 1",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
