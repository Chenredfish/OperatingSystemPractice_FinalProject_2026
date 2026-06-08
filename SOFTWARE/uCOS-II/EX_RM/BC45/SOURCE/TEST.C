/*
*********************************************************************************************************
* uC/OS-II
* The Real-Time Kernel
*
* OS Practice Final Project -- RM Scheduler
*********************************************************************************************************
*/

#include "includes.h"
#include <stdio.h> 

/*
*********************************************************************************************************
* CONSTANTS
*********************************************************************************************************
*/

#define  TASK_STK_SIZE   512
#define  MAX_TASKS         7

/*
*********************************************************************************************************
* VARIABLES
*********************************************************************************************************
*/

OS_STK  TaskStk[MAX_TASKS][TASK_STK_SIZE];
OS_STK  TaskStartStk[TASK_STK_SIZE];

INT32U  TaskPeriod[MAX_TASKS];
INT32U  TaskExecTime[MAX_TASKS];
int     TaskCount = 0;

INT32U  MyStartTime;

/*
*********************************************************************************************************
* FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void  TaskStart(void *pdata);
void  PeriodicTask(void *pdata);
static void  TaskStartCreateTasks(void);
static void  TaskStartDispInit(void);

/*
*********************************************************************************************************
* MAIN
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
* STARTUP TASK
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
* READ TASKSET AND CREATE TASKS
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
        TaskExecTime[i] = TaskExecTime[i] * OS_TICKS_PER_SEC;
        TaskPeriod[i]   = TaskPeriod[i] * OS_TICKS_PER_SEC;
    }
    fclose(fp);

    for (i = 0; i < TaskCount - 1; i++) {
        for (j = 0; j < TaskCount - i - 1; j++) {
            if (TaskPeriod[j] > TaskPeriod[j+1]) {
                tmpP = TaskPeriod[j];
                TaskPeriod[j] = TaskPeriod[j+1];
                TaskPeriod[j+1] = tmpP;
                tmpE = TaskExecTime[j];
                TaskExecTime[j] = TaskExecTime[j+1];
                TaskExecTime[j+1] = tmpE;
            }
        }
    }

    MyStartTime = OSTimeGet();

    for (i = 0; i < TaskCount; i++) {
        prio = i + 1;
        OSTaskCreateExt(PeriodicTask,
                        (void *)(INT32U)(i + 1),
                        &TaskStk[i][TASK_STK_SIZE - 1],
                        prio,
                        prio,
                        &TaskStk[i][0],
                        TASK_STK_SIZE,
                        (void *)0,
                        OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR,
                        TaskPeriod[i],
                        TaskExecTime[i]);

        sprintf(s, "Task%d: exec=%lds period=%lds prio=%d",
                i,
                TaskExecTime[i] / OS_TICKS_PER_SEC,
                TaskPeriod[i] / OS_TICKS_PER_SEC,
                prio);
        PC_DispStr(0, 5 + i, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    }
}

/*
*********************************************************************************************************
* PERIODIC TASK BODY
*********************************************************************************************************
*/

void PeriodicTask(void *pdata)
{
    char s[80];
    INT32U start_tick, end_tick, delay_ticks, next_arrival, run_count = 0;
    INT8U task_id = (INT8U)(INT32U)pdata;
    int display_id = task_id - 1;
    int row = 11 + display_id;
    INT32U start_sec, end_sec, period_sec, exec_sec;
    INT32U run_base, done_ticks, shown_ticks;

    next_arrival = MyStartTime + OSTCBCur->OSTCBPeriod;
    OSTCBCur->OSTCBDeadline = next_arrival;

    for (;;)
    {
        run_count++;
        start_tick = OSTimeGet();

        start_sec  = (start_tick - MyStartTime) / OS_TICKS_PER_SEC;
        period_sec = OSTCBCur->OSTCBPeriod   / OS_TICKS_PER_SEC;
        exec_sec   = OSTCBCur->OSTCBExecTime / OS_TICKS_PER_SEC;

        sprintf(s, "Task%d : start=%5lu  exec=%3lu/%3lu  end=----   period=%5lu  run=%4lu",
                display_id, (unsigned long)start_sec,
                (unsigned long)0, (unsigned long)exec_sec,
                (unsigned long)period_sec, (unsigned long)run_count);
        PC_DispStr(0, row, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

        /* Consume CPU time measured by the task's own run counter, which only      */
        /* advances while this task actually holds the CPU.  When preempted, the     */
        /* counter freezes, so progress pauses here and resumes after we are awoken. */
        run_base    = OSTCBCur->OSTCBRunCntr;
        shown_ticks = 0;
        while ((done_ticks = OSTCBCur->OSTCBRunCntr - run_base) < OSTCBCur->OSTCBExecTime) {
            if (done_ticks != shown_ticks) {               /* progressed by >=1 tick: refresh the row  */
                shown_ticks = done_ticks;
                sprintf(s, "Task%d : start=%5lu  exec=%3lu/%3lu  end=----   period=%5lu  run=%4lu",
                        display_id, (unsigned long)start_sec,
                        (unsigned long)(done_ticks / OS_TICKS_PER_SEC), (unsigned long)exec_sec,
                        (unsigned long)period_sec, (unsigned long)run_count);
                PC_DispStr(0, row, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
            }
        }

        end_tick = OSTimeGet();                            /* real wall-clock completion time          */
        end_sec  = (end_tick - MyStartTime) / OS_TICKS_PER_SEC;

        sprintf(s, "Task%d : start=%5lu  exec=%3lu/%3lu  end=%5lu  period=%5lu  run=%4lu",
                display_id, (unsigned long)start_sec,
                (unsigned long)exec_sec, (unsigned long)exec_sec,
                (unsigned long)end_sec, (unsigned long)period_sec, (unsigned long)run_count);
        PC_DispStr(0, row, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

        if (next_arrival > OSTimeGet()) {
            delay_ticks = next_arrival - OSTimeGet();
            OSTimeDly((INT16U)delay_ticks);
        } else {
            OSTimeDly(1);
        }

        next_arrival += OSTCBCur->OSTCBPeriod;
        OSTCBCur->OSTCBDeadline = next_arrival;
    }
}

/*
*********************************************************************************************************
* INITIALIZE THE DISPLAY
*********************************************************************************************************
*/

static void TaskStartDispInit (void)
{
    PC_DispStr(0,  0, "         uC/OS-II  --  RM Scheduler  --  OS Practice Final Project         ",
               DISP_FGND_WHITE + DISP_BGND_RED);
    PC_DispStr(0,  2, "Scheduler: Rate Monotonic (RM)  |  shorter period = higher priority",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
