/*
*********************************************************************************************************
* uC/OS-II
* The Real-Time Kernel
*
* OS Practice Final Project -- RM Scheduler
*
* Description: Reads taskset.txt, creates periodic tasks, and schedules them
* using Rate Monotonic (RM): shorter period = higher priority.
* Priority assignment is static and done at task creation time.
*
*********************************************************************************************************
*/

#include "includes.h"
#include <stdio.h> // 確保引入標準輸入輸出函式庫以支援讀檔

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

    // 1. 讀取 taskset.txt
    fp = fopen("taskset.txt", "r");
    if (fp == (FILE *)0) {
        PC_DispStr(0, 5, "ERROR: cannot open taskset.txt", DISP_FGND_RED + DISP_BGND_BLACK);
        return;
    }

    fscanf(fp, "%d", &TaskCount);
    for (i = 0; i < TaskCount; i++) {
        fscanf(fp, "%ld %ld", &TaskExecTime[i], &TaskPeriod[i]);
        // 換算為系統 Ticks
        TaskExecTime[i] = TaskExecTime[i] * OS_TICKS_PER_SEC;
        TaskPeriod[i]   = TaskPeriod[i] * OS_TICKS_PER_SEC;
    }
    fclose(fp);

    // 2. 氣泡排序：依 TaskPeriod 升冪排列，同步移動 TaskExecTime
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

    // 3. 建立 tasks，指派靜態優先權
    for (i = 0; i < TaskCount; i++) {
        prio = i + 1; // 陣列索引越小代表週期越短，優先權數字給予越小 (優先度越高)
        
        OSTaskCreateExt(PeriodicTask,
                        (void *)(INT32U)(i + 1),         // 將 i+1 作為 task_id 傳入
                        &TaskStk[i][TASK_STK_SIZE - 1],
                        prio,
                        prio,
                        &TaskStk[i][0],
                        TASK_STK_SIZE,
                        (void *)0,
                        OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR,
                        TaskPeriod[i],                   // 傳入週期
                        TaskExecTime[i]);                // 傳入執行時間

        // 顯示初始化資訊 (換算回秒數)
        sprintf(s, "Task%d: exec=%lds period=%lds prio=%d",
                i + 1,
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

void  PeriodicTask (void *pdata)
{
    INT32U  start_tick;
    INT32U  elapsed;
    INT32U  delay_ticks;
    INT8U   task_id;
    char    s[80];
    int     row;
    INT32U  run_count = 0;

    task_id = (INT8U)(INT32U)pdata;
    row     = 13 + (int)task_id;

    for (;;) {
        run_count++;
        start_tick = OSTimeGet();

        // 忙碌等待前：印出 start 狀態，end 留空
        sprintf(s, "Task%d | start=%5lu  end=----s  period=%5lu  run=%4lu",
                task_id,
                start_tick / OS_TICKS_PER_SEC,
                OSTCBCur->OSTCBPeriod / OS_TICKS_PER_SEC,
                run_count);
        PC_DispStr(0, row, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

        // 模擬執行：佔用 CPU 直到經過要求的執行時間
        while ((OSTimeGet() - start_tick) < OSTCBCur->OSTCBExecTime) {
            // Busy wait loop
        }

        elapsed = OSTimeGet() - start_tick;

        // 忙碌等待後：印出實際 end 時間
        sprintf(s, "Task%d | start=%5lu  end=%5lu  period=%5lu  run=%4lu",
                task_id,
                start_tick / OS_TICKS_PER_SEC,
                OSTimeGet() / OS_TICKS_PER_SEC,
                OSTCBCur->OSTCBPeriod / OS_TICKS_PER_SEC,
                run_count);
        PC_DispStr(0, row, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

        // 計算進入下一週期的延遲時間
        if (OSTCBCur->OSTCBPeriod > elapsed) {
            delay_ticks = OSTCBCur->OSTCBPeriod - elapsed;
            OSTimeDly((INT16U)delay_ticks);
        } else {
            OSTimeDly(1);
        }
    }
}

/*
*********************************************************************************************************
* INITIALIZE THE DISPLAY
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
