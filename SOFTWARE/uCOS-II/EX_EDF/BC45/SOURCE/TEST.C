/*
*********************************************************************************************************
*                                               uC/OS-II
*                                         The Real-Time Kernel
*
*                             OS Practice Final Project -- EDF Scheduler
*
* Description: Reads taskset.txt, creates periodic tasks, and demonstrates
*              Earliest Deadline First (EDF) scheduling.
*              OS_Sched() (modified via #define SCHED_EDF in OS_CFG.H) dynamically
*              assigns priority 1 to the ready task with the earliest absolute deadline.
*
* Teammate B implements the EDF scheduler logic in:
*   SOFTWARE/uCOS-II/SOURCE/OS_CORE.C  (inside #ifdef SCHED_EDF block in OS_Sched())
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
#define  BASE_PRIO        10    /* App tasks start at this priority; EDF gives prio 1 dynamically */

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
* EDF does NOT need tasks sorted by period.
* Assign stable priorities BASE_PRIO, BASE_PRIO+1, BASE_PRIO+2, ...
* OS_Sched() will dynamically promote the earliest-deadline ready task to priority 1.
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
        TaskExecTime[i] *= OS_TICKS_PER_SEC;
        TaskPeriod[i]   *= OS_TICKS_PER_SEC;
    }
    fclose(fp);

    for (i = 0; i < TaskCount; i++) {
        prio = (INT8U)(BASE_PRIO + i);
        OSTaskCreateExt(
            PeriodicTask,
            (void *)(INT32U)(i + 1),
            &TaskStk[i][TASK_STK_SIZE - 1],
            prio,
            prio,
            &TaskStk[i][0],
            TASK_STK_SIZE,
            (void *)0,
            OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR,
            TaskPeriod[i],
            TaskExecTime[i]
        );
        sprintf(s, "Task%d: exec=%lds  period=%lds  prio=%d",
                (int)(i+1), TaskExecTime[i]/OS_TICKS_PER_SEC,
                TaskPeriod[i]/OS_TICKS_PER_SEC, (int)prio);
        PC_DispStr(0, 5 + i, s, DISP_FGND_WHITE + DISP_BGND_BLACK);
    }
}

/*
*********************************************************************************************************
*                                           PERIODIC TASK BODY
*
* Each task busy-waits for its exec_time, then updates its deadline and sleeps.
* OSTCBDeadline MUST be updated BEFORE OSTimeDly() -- the scheduler reads it on next wakeup.
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
        start_tick = OSTimeGet();

        while ((OSTimeGet() - start_tick) < OSTCBCur->OSTCBExecTime) {
            ;
        }

        elapsed = OSTimeGet() - start_tick;

        sprintf(s, "[t=%4lds] Task%d  deadline=%lds  period=%lds",
                OSTimeGet()/OS_TICKS_PER_SEC, (int)task_id,
                OSTCBCur->OSTCBDeadline/OS_TICKS_PER_SEC,
                OSTCBCur->OSTCBPeriod/OS_TICKS_PER_SEC);
        PC_DispStr(0, row, s, DISP_FGND_CYAN + DISP_BGND_BLACK);

        OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;  /* must be before OSTimeDly */

        delay_ticks = OSTCBCur->OSTCBPeriod - elapsed;
        if ((INT32S)delay_ticks > 0) {
            OSTimeDly((INT16U)delay_ticks);
        }
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
