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
#include <string.h>

/* ---- Context-switch ring buffer (defined in OS_CPU_C.C) ---- */
#define CTX_LOG_SIZE  2048
typedef struct { INT32U time; INT8U from_id; INT8U to_id; INT8U from_done; } CTX_LOG_ENTRY;
extern CTX_LOG_ENTRY  CtxLog[CTX_LOG_SIZE];
extern INT16U         CtxLogCount;

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
int     TaskUsesSem[MAX_TASKS];
int     TaskCount = 0;
INT32U  MyStartTime;
INT32U  TaskRunBase[MAX_TASKS + 2];   /* indexed by OSTCBId; set before busy-wait, cleared after */

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

    {
        static INT32U seg_start_tick[MAX_TASKS + 2];   /* indexed by OSTCBId (1-based) */
        static int    disp_row   = 8;
        static INT16U render_idx = 0;

        for (;;) {
            INT16U log_snap = CtxLogCount;

            while (render_idx < log_snap) {
                CTX_LOG_ENTRY e   = CtxLog[render_idx];
                INT32U        t_s = (e.time >= MyStartTime)
                                    ? (e.time - MyStartTime) / OS_TICKS_PER_SEC : 0;
                char          line[80];

                /* Close (direct): user task voluntarily yields or is preempted by another user task */
                if (e.from_id >= 1 && e.from_id <= (INT8U)TaskCount
                    && seg_start_tick[e.from_id] != 0
                    && (e.to_id != 0 || e.from_done)) {
                    INT32U ss = (seg_start_tick[e.from_id] >= MyStartTime)
                                ? (seg_start_tick[e.from_id] - MyStartTime) / OS_TICKS_PER_SEC : 0;
                    if (t_s > ss) {
                        sprintf(line, "Task%d:  start=%3lus  end=%3lus%s",
                            (int)e.from_id,
                                (unsigned long)ss,
                                (unsigned long)t_s,
                                (!e.from_done) ? "  (preempted)" : "");
                        PC_DispStr(0, disp_row, line, DISP_FGND_WHITE + DISP_BGND_BLACK);
                        if (++disp_row > 23) disp_row = 8;
                    }
                    seg_start_tick[e.from_id] = 0;
                }

                /* Close (indirect): TaskStart resumes user task k -- any OTHER open user task
                 * was preempted via TaskStart; close it as preempted right now */
                if (e.from_id == 0 && e.to_id >= 1 && e.to_id <= (INT8U)TaskCount) {
                    int j;
                    for (j = 1; j <= (int)TaskCount; j++) {
                        if (j != (int)e.to_id && seg_start_tick[j] != 0) {
                            INT32U ss2 = (seg_start_tick[j] >= MyStartTime)
                                         ? (seg_start_tick[j] - MyStartTime) / OS_TICKS_PER_SEC : 0;
                            if (t_s > ss2) {
                                sprintf(line, "Task%d:  start=%3lus  end=%3lus  (preempted)",
                                    j,
                                    (unsigned long)ss2,
                                    (unsigned long)t_s);
                                PC_DispStr(0, disp_row, line, DISP_FGND_WHITE + DISP_BGND_BLACK);
                                if (++disp_row > 23) disp_row = 8;
                            }
                            seg_start_tick[j] = 0;
                        }
                    }
                }

                /* Open: only record start time once per segment (never overwrite an open segment) */
                if (e.to_id >= 1 && e.to_id <= (INT8U)TaskCount) {
                    if (seg_start_tick[e.to_id] == 0)
                        seg_start_tick[e.to_id] = e.time;
                }

                render_idx++;
            }

            if (PC_GetKey(&key) == TRUE) {
                if (key == 0x1B) {
                    PC_DOSReturn();
                }
            }
            OSTimeDlyHMSM(0, 0, 1, 0);
        }
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
    int     i, j, tmpS;
    INT32U  tmpP, tmpE;
    INT8U   prio;

    fp = fopen("taskset.txt", "r");
    if (fp == (FILE *)0) {
        PC_DispStr(0, 5, "ERROR: cannot open taskset.txt", DISP_FGND_RED + DISP_BGND_BLACK);
        return;
    }

    fscanf(fp, "%d", &TaskCount);
    for (i = 0; i < TaskCount; i++) {
        fscanf(fp, "%ld %ld %d", &TaskExecTime[i], &TaskPeriod[i], &TaskUsesSem[i]);
        TaskExecTime[i] *= OS_TICKS_PER_SEC;
        TaskPeriod[i]   *= OS_TICKS_PER_SEC;
    }
    fclose(fp);

    for (i = 0; i < TaskCount - 1; i++) {
        for (j = 0; j < TaskCount - 1 - i; j++) {
            if (TaskPeriod[j] > TaskPeriod[j+1]) {
                tmpP = TaskPeriod[j];   TaskPeriod[j]   = TaskPeriod[j+1];   TaskPeriod[j+1]   = tmpP;
                tmpE = TaskExecTime[j]; TaskExecTime[j] = TaskExecTime[j+1]; TaskExecTime[j+1] = tmpE;
                tmpS = TaskUsesSem[j];  TaskUsesSem[j]  = TaskUsesSem[j+1];  TaskUsesSem[j+1]  = tmpS;
            }
        }
    }

    MyStartTime = OSTimeGet();

    for (i = 0; i < TaskCount; i++) {
        prio = (INT8U)(i + 2);  /* prio=1 reserved as PCP ceiling; user tasks start at 2 */
        OSTaskCreateExt(
            PeriodicTask,
            (void *)(INT32U)(i + 1),
            &TaskStk[i][TASK_STK_SIZE - 1],
            prio,
            (INT8U)(i + 1),     /* stable ID; PCP may temporarily set prio=1 but OSTCBId stays */
            &TaskStk[i][0],
            TASK_STK_SIZE,
            (void *)0,
            OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR,
            TaskPeriod[i],
            TaskExecTime[i]
        );
    }

    SharedSem = OSSemCreate(1);
    SharedSem->OSEventCeiling = 1;          /* PCP: prio=1 reserved as ceiling, always free */
    SharedSem->OSEventOwner   = (void *)0;  /* SharedSem starts unlocked                    */

    {
        char info[160];
        char chunk[32];
        int  col      = 0;
        int  info_row = 5;
        info[0] = '\0';
        for (i = 0; i < TaskCount; i++) {
            sprintf(chunk, "T%d:e=%lds,p=%lds,sem=%c  ",
                    i + 1,
                    (long)(TaskExecTime[i] / OS_TICKS_PER_SEC),
                    (long)(TaskPeriod[i]   / OS_TICKS_PER_SEC),
                    TaskUsesSem[i] ? 'Y' : 'N');
            if (col + (int)strlen(chunk) > 78) {
                PC_DispStr(0, info_row, info, DISP_FGND_WHITE + DISP_BGND_BLACK);
                info[0] = '\0';
                col = 0;
                info_row++;
            }
            strcat(info, chunk);
            col += (int)strlen(chunk);
        }
        if (info[0])
            PC_DispStr(0, info_row, info, DISP_FGND_WHITE + DISP_BGND_BLACK);
    }
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
    INT32U  release_tick;
    INT32U  run_base;
    INT32U  end_tick;
    INT32U  delay_ticks;
    INT32U  period_ticks;
    INT32U  exec_ticks;
    INT8U   task_id;
    INT8U   err;

    task_id      = (INT8U)(INT32U)pdata;
    period_ticks = OSTCBCur->OSTCBPeriod;
    exec_ticks   = OSTCBCur->OSTCBExecTime;
    release_tick = MyStartTime;

    for (;;) {
        if (TaskUsesSem[task_id - 1])
            OSSemPend(SharedSem, 0, &err);

        /* Snapshot AFTER semaphore: we measure CPU time spent in the critical section only */
        run_base = OSTCBCur->OSTCBRunCntr;
        TaskRunBase[OSTCBCur->OSTCBId] = run_base;   /* expose to hook for work-done detection */
        while ((OSTCBCur->OSTCBRunCntr - run_base) < exec_ticks) { }

        end_tick = OSTimeGet();

        if (TaskUsesSem[task_id - 1])
            OSSemPost(SharedSem);

        TaskRunBase[OSTCBCur->OSTCBId] = 0xFFFFFFFFu;           /* clear after release so hook can see completion */

        /* Advance release_tick to next period boundary after end_tick */
        release_tick += period_ticks;
        while (release_tick <= end_tick)
            release_tick += period_ticks;

        delay_ticks = release_tick - end_tick;
        while (delay_ticks > 60000) {
            OSTimeDly(60000);
            delay_ticks -= 60000;
        }
        if ((INT32S)delay_ticks > 0)
            OSTimeDly((INT16U)delay_ticks);
    }
}

/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDispInit (void)
{
    PC_DispStr(0,  0, "    uC/OS-II  --  RM + PCP (Fixed Handoff)  --  OS Practice Final Project    ",
               DISP_FGND_WHITE + DISP_BGND_RED);
    PC_DispStr(0,  2, "Scheduler: RM  |  Bonus: Priority Ceiling Protocol on shared resource",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0,  4, "Task Config:",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0,  7, "--- Context Switch Trace: each line = one CPU segment (seconds) ---",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
