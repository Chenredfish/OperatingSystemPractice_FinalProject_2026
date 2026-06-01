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
* We read all tasks, sort by period ascending, then assign priority 1, 2, 3...
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

    /* Sort by period ascending -- shortest period gets priority 1 */
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
        sprintf(s, "Task%d: exec=%ld period=%ld prio=%d", (int)(i+1), TaskExecTime[i], TaskPeriod[i], (int)prio);
        PC_DispStr(0, 5 + i, s, DISP_FGND_WHITE + DISP_BGND_BLACK);
    }
}

/*
*********************************************************************************************************
*                                           PERIODIC TASK BODY
*
* Each task:
*   1. Records start tick
*   2. Busy-waits for OSTCBExecTime ticks (simulates computation, can be preempted)
*   3. Prints which task ran and when
*   4. Calls OSTimeDly() to sleep until next period
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

        sprintf(s, "[t=%4ld] Task%d ran  exec=%ld period=%ld",
                OSTimeGet(), (int)task_id, OSTCBCur->OSTCBExecTime, OSTCBCur->OSTCBPeriod);
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
    PC_DispStr(0,  0, "         uC/OS-II  --  RM Scheduler  --  OS Practice Final Project         ",
               DISP_FGND_WHITE + DISP_BGND_RED);
    PC_DispStr(0,  2, "Scheduler: Rate Monotonic (RM)  |  shorter period = higher priority",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
