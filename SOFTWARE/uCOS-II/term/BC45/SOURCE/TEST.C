#include "includes.h"
#include <time.h>

#define TASK_STK_SIZE 512

OS_STK TaskStkNormal[TASK_STK_SIZE];
OS_STK TaskStkEmergency[TASK_STK_SIZE];
OS_STK TaskStkGame[TASK_STK_SIZE];
OS_STK TaskStkKey[TASK_STK_SIZE];
OS_STK TaskStartStk[TASK_STK_SIZE];

OS_EVENT *RandomSem;
OS_EVENT *ModeSem;

volatile INT8U mode = 0;
volatile INT8U last_number = 0;
volatile INT8U target_number = 0;
volatile INT8U game_score = 0;
volatile INT8U game_attempts = 0;
volatile INT8U player_guess = 0;
volatile INT8U new_guess = 0;
volatile INT8U prev_mode = 255;
char alive_sym = '-';

void TaskStart(void *pdata);
void TaskNormal(void *pdata);
void TaskEmergency(void *pdata);
void TaskGame(void *pdata);
void TaskKey(void *pdata);
static void InitScreen(void);
static void TaskStartDisp(void);

void main(void)
{
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);
    OSInit();
    PC_DOSSaveReturn();
    PC_VectSet(uCOS, OSCtxSw);
    RandomSem = OSSemCreate(1);
    ModeSem = OSSemCreate(1);
    OSTaskCreate(TaskStart, (void *)0, &TaskStartStk[TASK_STK_SIZE - 1], 0);
    OSStart();
}

void TaskStart(void *pdata)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR cpu_sr;
#endif
    INT16S key;
    
    pdata = pdata;
    srand((unsigned int)time(NULL));  // 初始化隨機數種子
    InitScreen();
    
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();
    
    OSStatInit();
    
    OSTaskCreate(TaskNormal, (void *)0, &TaskStkNormal[TASK_STK_SIZE - 1], 2);
    OSTaskCreate(TaskEmergency, (void *)0, &TaskStkEmergency[TASK_STK_SIZE - 1], 3);
    OSTaskCreate(TaskGame, (void *)0, &TaskStkGame[TASK_STK_SIZE - 1], 4);
    OSTaskCreate(TaskKey, (void *)0, &TaskStkKey[TASK_STK_SIZE - 1], 5);
    
    for (;;) {
        TaskStartDisp();
        if (PC_GetKey(&key) == TRUE) {
            if (key == 0x1B) {
                PC_DOSReturn();
            }
        }
        OSCtxSwCtr = 0;
        OSTimeDlyHMSM(0,0,0,200);
    }
}

static void InitScreen(void)
{
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);
    
    PC_DispStr(0, 0, "                         uC/OS-II, The Real-Time Kernel                         ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
    PC_DispStr(0, 1, "                                Jean J. Labrosse                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 3, "                                 THREE-MODE CONTROL                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 22, "#Tasks          :         CPU Usage:     %                                       ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr(0, 24, "                <-ESC:QUIT, F:FIX, G:GAME, 1-0:GUESS, Q:QUIT GAME->               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
    
    PC_DispStr(0, 5, "Student ID:  B1229057", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 6, "Alive:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 7, "Random number:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 8, "Mode:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 9, "Emergency countdown:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 10, "Game Status:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 11, "Score/Attempts:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr(0, 12, "Your Guess:", DISP_FGND_YELLOW + DISP_BGND_BLUE);
}

static void TaskStartDisp(void)
{
    char s[32];
    INT8U cur_mode, val, err;
    
    OSSemPend(ModeSem, 0, &err);
    val = last_number;
    cur_mode = mode;
    
    if (cur_mode != prev_mode) {
        InitScreen();
        prev_mode = cur_mode;
    }
    OSSemPost(ModeSem);
    
    sprintf(s, "%5d", OSTaskCtr);
    PC_DispStr(18, 22, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#if OS_TASK_STAT_EN > 0
    sprintf(s, "%3d", OSCPUUsage);
    PC_DispStr(36, 22, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#endif
    sprintf(s, "V%1d.%02d", OSVersion() / 100, OSVersion() % 100);
    PC_DispStr(75, 24, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
    
    alive_sym = (alive_sym == '-') ? '\\' : ((alive_sym == '\\') ? '|' : ((alive_sym == '|') ? '/' : '-'));
    sprintf(s, "%c", alive_sym);
    PC_DispStr(7, 6, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
    
    sprintf(s, "%2d", val);
    PC_DispStr(15, 7, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
    
    if (cur_mode == 0) {
        PC_DispStr(6, 8, "NORMAL   ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    } else if (cur_mode == 1) {
        PC_DispStr(6, 8, "EMERGENCY", DISP_FGND_WHITE + DISP_BGND_RED);
    } else if (cur_mode == 2) {
        PC_DispStr(6, 8, "GAME MODE", DISP_FGND_BLACK + DISP_BGND_GREEN);
    }
}

void TaskNormal(void *pdata)
{
    INT8U num, err;
    
    pdata = pdata;
    for (;;) {
        OSSemPend(ModeSem, 0, &err);
        if (mode == 0) {
            OSSemPost(ModeSem);
            
            // 生成隨機數
            OSSemPend(RandomSem, 0, &err);
            num = (INT8U)(rand() % 10 + 1);
            OSSemPost(RandomSem);
            
            // 更新顯示和檢查模式切換
            OSSemPend(ModeSem, 0, &err);
            last_number = num;
            
            if (num <= 2) {
                mode = 1;  // 緊急模式
            } else if (num == 7) {
                mode = 2;  // 遊戲模式
                target_number = (INT8U)(rand() % 10 + 1);
                game_attempts = 0;
                game_score = 0;
                player_guess = 0;
                new_guess = 0;
            }
            OSSemPost(ModeSem);
            
            OSTimeDlyHMSM(0,0,1,0);
        } else {
            OSSemPost(ModeSem);
            OSTimeDlyHMSM(0,0,0,100);
        }
    }
}

void TaskEmergency(void *pdata)
{
    INT8U remaining, err, i;
    char s[50];
    
    pdata = pdata;
    for (;;) {
        OSSemPend(ModeSem, 0, &err);
        if (mode == 1) {
            OSSemPost(ModeSem);
            
            // 5秒倒數
            PC_DispStr(20, 9, "     ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
            for (remaining = 5; remaining > 0; remaining--) {
                sprintf(s, "%d", remaining);
                PC_DispStr(20, 9, "  ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
                PC_DispStr(20, 9, s, DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
                
                OSTimeDlyHMSM(0,0,1,0);
                
                OSSemPend(ModeSem, 0, &err);
                if (mode != 1) {
                    PC_DispStr(20, 9, "     ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
                    OSSemPost(ModeSem);
                    break;
                }
                OSSemPost(ModeSem);
            }
            
            // 檢查是否需要關機
            OSSemPend(ModeSem, 0, &err);
            if (mode == 1 && remaining == 0) {
                OSSemPost(ModeSem);
                
                PC_DispStr(20, 9, "     ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
                PC_DispStr(25, 13, "*** System Failure ***", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
                
                for (i = 3; i > 0; i--) {
                    sprintf(s, "System shutdown in %d seconds", i);
                    PC_DispStr(25, 14, s, DISP_FGND_YELLOW + DISP_BGND_RED + DISP_BLINK);
                    OSTimeDlyHMSM(0,0,1,0);
                }
                PC_DOSReturn();
            } else {
                OSSemPost(ModeSem);
            }
        } else {
            OSSemPost(ModeSem);
            OSTimeDlyHMSM(0,0,0,200);
        }
    }
}

void TaskGame(void *pdata)
{
    INT8U err;
    char s[32];
    
    pdata = pdata;
    for (;;) {
        OSSemPend(ModeSem, 0, &err);
        if (mode == 2) {
            // 遊戲狀態顯示
            PC_DispStr(13, 10, "Find 1-10!    ", DISP_FGND_BLACK + DISP_BGND_GREEN);
            
            sprintf(s, "%d/%d  ", game_score, game_attempts);
            PC_DispStr(16, 11, s, DISP_FGND_BLACK + DISP_BGND_GREEN);
            
            if (player_guess > 0) {
                if (player_guess == 10) {
                    sprintf(s, "10");
                } else {
                    sprintf(s, "%d", player_guess);
                }
                PC_DispStr(12, 12, s, DISP_FGND_WHITE + DISP_BGND_GREEN);
            } else {
                PC_DispStr(12, 12, "?", DISP_FGND_WHITE + DISP_BGND_GREEN);
            }
            
            // 處理新猜測
            if (new_guess && player_guess > 0) {
                game_attempts++;
                new_guess = 0;
                
                if (player_guess == target_number) {
                    game_score++;
                    PC_DispStr(13, 10, "CORRECT!  +1  ", DISP_FGND_WHITE + DISP_BGND_GREEN + DISP_BLINK);
                    target_number = (INT8U)(rand() % 10 + 1);
                    player_guess = 0;
                    
                    if (game_score >= 3) {
                        PC_DispStr(13, 10, "YOU WIN! 3/3  ", DISP_FGND_WHITE + DISP_BGND_GREEN + DISP_BLINK);
                        OSSemPost(ModeSem);
                        OSTimeDlyHMSM(0,0,2,0);
                        OSSemPend(ModeSem, 0, &err);
                        mode = 0;
                        game_score = 0;
                    }
                } else {
                    if (player_guess < target_number) {
                        PC_DispStr(13, 10, "TOO LOW!      ", DISP_FGND_YELLOW + DISP_BGND_RED);
                    } else {
                        PC_DispStr(13, 10, "TOO HIGH!     ", DISP_FGND_YELLOW + DISP_BGND_RED);
                    }
                    
                    if (game_attempts >= 10) {
                        PC_DispStr(13, 10, "GAME OVER!    ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
                        OSSemPost(ModeSem);
                        OSTimeDlyHMSM(0,0,2,0);
                        OSSemPend(ModeSem, 0, &err);
                        mode = 0;
                        game_score = 0;
                    }
                }
            }
            OSSemPost(ModeSem);
        } else {
            OSSemPost(ModeSem);
        }
        OSTimeDlyHMSM(0,0,0,200);
    }
}

void TaskKey(void *pdata)
{
    INT16S key;
    INT8U err;
    
    pdata = pdata;
    for (;;) {
        if (PC_GetKey(&key) == TRUE) {
            if ((key == 'f') || (key == 'F')) {
                OSSemPend(ModeSem, 0, &err);
                if (mode == 1) {
                    mode = 0;
                }
                OSSemPost(ModeSem);
            } else if ((key == 'g') || (key == 'G')) {
                OSSemPend(ModeSem, 0, &err);
                if (mode == 0) {
                    mode = 2;
                    target_number = (INT8U)(random(10) + 1);
                    game_attempts = 0;
                    game_score = 0;
                    player_guess = 0;
                    new_guess = 0;
                }
                OSSemPost(ModeSem);
            } else if ((key == 'q') || (key == 'Q')) {
                OSSemPend(ModeSem, 0, &err);
                if (mode == 2) {
                    mode = 0;
                    game_score = 0;
                }
                OSSemPost(ModeSem);
            } else if (key >= '1' && key <= '9') {
                OSSemPend(ModeSem, 0, &err);
                if (mode == 2) {
                    player_guess = key - '0';
                    new_guess = 1;
                }
                OSSemPost(ModeSem);
            } else if (key == '0') {
                OSSemPend(ModeSem, 0, &err);
                if (mode == 2) {
                    player_guess = 10;
                    new_guess = 1;
                }
                OSSemPost(ModeSem);
            }
        }
        OSTimeDlyHMSM(0,0,0,100);
    }
}