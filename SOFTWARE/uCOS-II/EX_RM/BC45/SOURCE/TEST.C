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
#include <string.h>

/* ---- Context-switch ring buffer (defined in OS_CPU_C.C) ---- */
#define CTX_LOG_SIZE  2048
typedef struct { INT32U time; INT8U from_id; INT8U to_id; INT8U from_done; } CTX_LOG_ENTRY;
extern CTX_LOG_ENTRY  CtxLog[CTX_LOG_SIZE];
extern INT16U         CtxLogCount;

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
INT32U  TaskRunBase[MAX_TASKS + 2];   /* indexed by OSTCBId; set before busy-wait, cleared after */

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

    {
        static INT32U seg_start_tick[MAX_TASKS + 2];   /* indexed by OSTCBId (1-based) */
        static int    disp_row  = 8;
        static INT16U render_idx = 0;

        for (;;) {
            INT16U log_snap = CtxLogCount;   /* read once to avoid partial INT16U read */

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
* READ TASKSET AND CREATE TASKS
*********************************************************************************************************
*/

static  void  TaskStartCreateTasks (void)
{
    FILE   *fp;
    int     i, j;
    INT32U  tmpP, tmpE;
    INT8U   prio;

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
    }

    {
        char info[160];
        char chunk[24];
        int  col      = 0;
        int  info_row = 5;
        info[0] = '\0';
        for (i = 0; i < TaskCount; i++) {
            sprintf(chunk, "T%d:e=%lds,p=%lds  ",
                    i,
                    (long)(TaskExecTime[i] / OS_TICKS_PER_SEC),
                    (long)(TaskPeriod[i]   / OS_TICKS_PER_SEC));
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
* PERIODIC TASK BODY
*********************************************************************************************************
*/

void PeriodicTask(void *pdata)
{
    INT32U  next_arrival;
    INT32U  run_base;
    INT32U  delay_ticks;

    next_arrival            = MyStartTime + OSTCBCur->OSTCBPeriod;
    OSTCBCur->OSTCBDeadline = next_arrival;

    for (;;) {
        /* OSTCBRunCntr freezes during preemption -- correctly measures CPU time */
        run_base = OSTCBCur->OSTCBRunCntr;
        TaskRunBase[OSTCBCur->OSTCBId] = run_base;   /* expose to hook for work-done detection */
        while ((OSTCBCur->OSTCBRunCntr - run_base) < OSTCBCur->OSTCBExecTime) { }
        TaskRunBase[OSTCBCur->OSTCBId] = 0;           /* clear: hook must not re-trigger */

        /* Sleep until next absolute release (chunked to avoid INT16U overflow) */
        if (next_arrival > OSTimeGet()) {
            delay_ticks = next_arrival - OSTimeGet();
            while (delay_ticks > 60000) {
                OSTimeDly(60000);
                delay_ticks -= 60000;
            }
            OSTimeDly((INT16U)delay_ticks);
        } else {
            OSTimeDly(1);
        }

        next_arrival           += OSTCBCur->OSTCBPeriod;
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
    PC_DispStr(0,  4, "Task Config:",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0,  7, "--- Context Switch Trace: each line = one CPU segment (seconds) ---",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                         <-- PRESS ESC TO QUIT -->                          ",
               DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
