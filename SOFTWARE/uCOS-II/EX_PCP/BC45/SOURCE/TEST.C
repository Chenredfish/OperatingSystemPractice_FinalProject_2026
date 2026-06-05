/*
*********************************************************************************************************
*                                               uC/OS-II
*                                         The Real-Time Kernel
*
*                          OS Practice Final Project -- RM + PCP (Bonus)
*
* Description: RM scheduler base (same as EX_RM) with shared resource access.
*              Tasks compete for a shared semaphore to demonstrate priority inversion.
*              Teammate C implements PCP in OS_SEM.C (OSSemPend / OSSemPost).
*
* Semaphore setup:
*   SharedSem->OSEventCeiling = highest-priority task that uses this semaphore
*   (smallest priority number among all users of the semaphore).
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

OS_STK    TaskStk[MAX_TASKS][TASK_STK_SIZE];
OS_STK    TaskStartStk[TASK_STK_SIZE];
OS_EVENT *SharedSem;

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
* Same RM sorting as EX_RM. Also creates SharedSem for PCP demonstration.
*
* TEAMMATE C: after OSSemCreate(), set the ceiling:
*   SharedSem->OSEventCeiling = <priority of highest-prio task that uses this sem>;
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
        TaskExecTime[i] *= OS_TICKS_PER_SEC;
        TaskPeriod[i]   *= OS_TICKS_PER_SEC;
    }
    fclose(fp);

    for (i = 0; i < TaskCount - 1; i++) {
        for (j = 0; j < TaskCount - 1 - i; j++) {
            if (TaskPeriod[j] > TaskPeriod[j+1]) {
                tmpP = TaskPeriod[j];   TaskPeriod[j]   = TaskPeriod[j+1];   TaskPeriod[j+1]   = tmpP;
                tmpE = TaskExecTime[j]; TaskExecTime[j] = TaskExecTime[j+1]; TaskExecTime[j+1] = tmpE;
            }
        }
    }

    for (i = 0; i < TaskCount; i++) {
        prio = (INT8U)(i + 1);
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
        sprintf(s, "Task%d: exec=%lds period=%lds prio=%d",
                (int)(i+1), TaskExecTime[i]/OS_TICKS_PER_SEC,
                TaskPeriod[i]/OS_TICKS_PER_SEC, (int)prio);
        PC_DispStr(0, 5 + i, s, DISP_FGND_WHITE + DISP_BGND_BLACK);
    }

    SharedSem = OSSemCreate(1);
    /* TEAMMATE C: SharedSem->OSEventCeiling = 1; */
}

/*
*********************************************************************************************************
*                                           PERIODIC TASK BODY
*
* Tasks acquire SharedSem before their critical section.
* Without PCP: a low-priority task holding SharedSem can block a high-priority task.
* With PCP (implemented by Teammate C in OSSemPend/OSSemPost): this is prevented.
*
* This file does NOT need to change for PCP -- implement it only in OS_SEM.C.
*********************************************************************************************************
*/

void  PeriodicTask (void *pdata)
{
    INT32U  start_tick;
    INT32U  elapsed;
    INT32U  delay_ticks;
    INT8U   task_id;
    INT8U   err;
    char    s[80];
    int     row;

    task_id = (INT8U)(INT32U)pdata;
    row     = 13 + (int)task_id;

    for (;;) {
        start_tick = OSTimeGet();

        OSSemPend(SharedSem, 0, &err);

        while ((OSTimeGet() - start_tick) < OSTCBCur->OSTCBExecTime) {
            ;
        }

        OSSemPost(SharedSem);

        elapsed = OSTimeGet() - start_tick;

        sprintf(s, "[t=%4lds] Task%d ran  exec=%lds period=%lds",
                OSTimeGet()/OS_TICKS_PER_SEC, (int)task_id,
                OSTCBCur->OSTCBExecTime/OS_TICKS_PER_SEC,
                OSTCBCur->OSTCBPeriod/OS_TICKS_PER_SEC);
        PC_DispStr(0, row, s, DISP_FGND_YELLOW + DISP_BGND_BLACK);

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
    PC_DispStr(0,  0, "       uC/OS-II  --  RM + PCP (Bonus)  --  OS Practice Final Project        ",
               DISP_FGND_WHITE + DISP_BGND_RED);
    PC_DispStr(0,  2, "Scheduler: RM  |  Bonus: Priority Ceiling Protocol on shared resource",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
