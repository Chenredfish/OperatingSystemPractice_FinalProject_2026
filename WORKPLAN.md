# 作業系統期末專題 — 工作分配說明

> 長庚大學 作業系統實習 Final Project  
> 截止日：2026/06/08 23:00 | Demo：2026/06/11

---

## 一、共同必讀（所有人都要了解）

### 1-1 uC/OS II 排程機制

uC/OS II 是**優先權驅動**的即時作業系統，核心規則只有一條：

> **優先權數字越小，優先權越高。排程器永遠選擇優先權數字最小的 ready task 執行。**

我們要實作的 RM 和 EDF，本質上都是「在每次做排程決定前，動態計算誰應該有最高優先權，然後把那個 task 的優先權數字改成最小」。

### 1-2 Task 的生命週期

```
建立 (OSTaskCreateExt)
    ↓
Ready（就緒）
    ↓
Running（執行中）←─────────────────────┐
    ↓                                   │
完成一個執行週期                         │
    ↓                                   │
更新 deadline（EDF 需要）               │
    ↓                                   │
OSTimeDly() → Delayed（等待下一個週期）  │
    ↓                                   │
時間到 → Ready ─────────────────────────┘
```

### 1-3 所有人都必須知道的關鍵函數

| 函數 | 所在檔案 | 說明 |
|------|----------|------|
| `OS_Sched()` | os_core.c | **最核心**：決定下一個執行的 task，每次可能發生 context switch |
| `OSTimeTick()` | os_core.c | 每個 timer tick 呼叫一次，負責將 delayed task 倒計時 |
| `OSIntExit()` | os_core.c | 中斷結束後觸發重新排程 |
| `OSTaskCreateExt()` | os_task.c | 建立 task，我們會修改它來傳入 period 和 exec_time |
| `OSTaskChangePrio(oldprio, newprio)` | os_task.c | 動態修改某個 task 的優先權數字 |
| `OSTimeDly(ticks)` | os_time.c | 讓目前執行中的 task 暫停指定 ticks，進入 delayed 狀態 |
| `OSTimeGet()` | os_time.c | 取得目前系統時間（單位：ticks） |
| `OSStart()` | os_core.c | 啟動 OS，開始執行 tasks |

### 1-4 修改後的 OS_TCB 結構（所有人的程式都依賴這個）

`OS_TCB` 是每個 task 的控制區塊（Task Control Block），定義在 `ucos_ii.h`。  
基礎架構的人會加入以下三個欄位，**其他人在程式中可以直接用**：

```c
OSTCBCur->OSTCBPeriod    // 這個 task 的週期（ticks）
OSTCBCur->OSTCBExecTime  // 這個 task 的執行時間（ticks）
OSTCBCur->OSTCBDeadline  // 這個 task 的絕對截止時間（ticks），EDF 使用
OSTCBCur->OSTCBPrioOrg   // 這個 task 的原始優先權（PCP 使用）
```

`OSTCBCur` 是 uC/OS II 內建的全域指標，永遠指向**目前正在執行的 task 的 TCB**。

### 1-5 RM vs EDF 核心差異

| | RM（Rate Monotonic） | EDF（Earliest Deadline First） |
|--|----------------------|-------------------------------|
| 排程依據 | **週期**（靜態，不會變） | **絕對截止時間**（動態，每個週期更新） |
| 誰的優先權最高 | `OSTCBPeriod` 最小的 task | `OSTCBDeadline` 最小的 task |
| 優先權幾時重新計算 | 每次呼叫 OS_Sched() | 每次呼叫 OS_Sched() |

### 1-6 taskset.txt 格式

```
4          ← task 數量
1 12       ← task 1：執行時間=1, 週期=12
1 7        ← task 2：執行時間=1, 週期=7
2 19       ← task 3：執行時間=2, 週期=19
3 20       ← task 4：執行時間=3, 週期=20
```

---

## 二、基礎架構（負責人：你）

> **其他人的工作都依賴這部分，請優先完成。**  
> 完成後 merge 進 main，其他人再從 main 開 branch。

### 工作清單

- [ ] 修改 `ucos_ii.h`：在 OS_TCB 加入新欄位
- [ ] 修改 `os_task.c`：更新 `OSTaskCreateExt()` 簽名與初始化
- [ ] 修改 `main.c`：讀取 taskset.txt 並建立 tasks
- [ ] 定義共用的 task 骨架函數

---

### 步驟 1：修改 OS_TCB 結構

**檔案：** `ucos_ii.h`

找到 `OS_TCB` struct（搜尋 `typedef struct os_tcb`），在最後一個欄位後面加入：

```c
typedef struct os_tcb {
    // ... 原有欄位（不要動）...

    /* 新增欄位 */
    INT32U  OSTCBPeriod;    /* task 週期，單位 ticks                    */
    INT32U  OSTCBExecTime;  /* task 執行時間，單位 ticks                */
    INT32U  OSTCBDeadline;  /* task 絕對截止時間，EDF 使用              */
    INT8U   OSTCBPrioOrg;   /* task 原始優先權，PCP 恢復優先權時使用    */
} OS_TCB;
```

---

### 步驟 2：修改 OSTaskCreateExt()

**檔案：** `os_task.c`

**原始函數簽名：**
```c
INT8U  OSTaskCreateExt (void   (*task)(void *pd),
                        void    *pdata,
                        OS_STK  *ptos,
                        INT8U    prio,
                        INT16U   id,
                        OS_STK  *pbos,
                        INT32U   stk_size,
                        void    *pext,
                        INT16U   opt)
```

**修改後（加入兩個參數在最後）：**
```c
INT8U  OSTaskCreateExt (void   (*task)(void *pd),
                        void    *pdata,
                        OS_STK  *ptos,
                        INT8U    prio,
                        INT16U   id,
                        OS_STK  *pbos,
                        INT32U   stk_size,
                        void    *pext,
                        INT16U   opt,
                        INT32U   period,      /* 新增：task 週期 */
                        INT32U   exec_time)   /* 新增：task 執行時間 */
```

在函數內部，找到建立 TCB 之後的地方（搜尋 `OS_TCBInit`），加入初始化：

```c
// OS_TCBInit() 呼叫之後加入：
ptcb->OSTCBPeriod   = period;
ptcb->OSTCBExecTime = exec_time;
ptcb->OSTCBDeadline = period;   /* 初始 deadline = 第一個週期結束時 */
ptcb->OSTCBPrioOrg  = prio;     /* 記錄原始優先權 */
```

> **注意：** `ucos_ii.h` 中也有 `OSTaskCreateExt` 的函數宣告，記得同步修改那邊的參數列表。

---

### 步驟 3：main.c 讀取 taskset.txt 並建立 tasks

**檔案：** `main.c`（或環境中對應的應用程式入口）

```c
#include <stdio.h>
#include "includes.h"

#define TASK_STK_SIZE  512
#define MAX_TASKS      7

OS_STK TaskStk[MAX_TASKS][TASK_STK_SIZE];

INT32U TaskPeriod[MAX_TASKS];
INT32U TaskExecTime[MAX_TASKS];
int    TaskCount = 0;

void TaskFunc(void *pdata);  /* 宣告，定義在步驟 4 */

int main(void) {
    OSInit();  /* 初始化 uC/OS II */

    /* 讀取 taskset.txt */
    FILE *fp = fopen("taskset.txt", "r");
    if (fp == NULL) {
        printf("Error: cannot open taskset.txt\n");
        return -1;
    }

    fscanf(fp, "%d", &TaskCount);  /* 第一行：task 數量 */
    for (int i = 0; i < TaskCount; i++) {
        fscanf(fp, "%d %d", &TaskExecTime[i], &TaskPeriod[i]);
    }
    fclose(fp);

    /* 建立 tasks */
    for (int i = 0; i < TaskCount; i++) {
        OSTaskCreateExt(
            TaskFunc,
            (void *)(INT32U)i,                   /* 傳入 task 編號 */
            &TaskStk[i][TASK_STK_SIZE - 1],       /* stack 頂端 */
            i + 1,                                /* 初始優先權 1,2,3... */
            i + 1,                                /* task ID */
            &TaskStk[i][0],                       /* stack 底端 */
            TASK_STK_SIZE,
            (void *)0,
            OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR,
            TaskPeriod[i],                        /* 新增參數 */
            TaskExecTime[i]                       /* 新增參數 */
        );
    }

    OSStart();  /* 開始執行，不會 return */
    return 0;
}
```

---

### 步驟 4：定義共用 Task 骨架

```c
void TaskFunc(void *pdata) {
    int      task_id   = (int)(INT32U)pdata;
    INT32U   start_tick;
    INT32U   exec_ticks;
    INT32U   delay_ticks;

    for (;;) {
        start_tick = OSTimeGet();

        /* 模擬執行：busy-wait 直到執行時間到 */
        exec_ticks = OSTCBCur->OSTCBExecTime;
        while ((OSTimeGet() - start_tick) < exec_ticks) {
            ;  /* 等待 */
        }

        /* 更新下一個絕對 deadline（EDF 需要；RM 也保留，不影響正確性） */
        OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;

        /* 計算還要等多少 ticks 才到下一個週期 */
        delay_ticks = OSTCBCur->OSTCBPeriod - (OSTimeGet() - start_tick);
        if ((INT32S)delay_ticks > 0) {
            OSTimeDly(delay_ticks);
        }
    }
}
```

---

## 三、組員 A 的工作（RM 排程器）

> **Branch 名稱：** `feature/rm`  
> **從 main 開 branch（等基礎架構 merge 後才開始）**

### 目標

修改 `OS_Sched()`，讓每次排程前先把「週期最短的 ready task」的優先權數字設為最小，讓 uC/OS II 的原有排程邏輯自然選到它。

### 需要動的檔案

- `os_core.c`（修改 `OS_Sched()`）

### 實作說明

**檔案：** `os_core.c`

找到 `void  OS_Sched (void)` 函數定義，在進入臨界區（`OS_ENTER_CRITICAL()`）後、原有排程邏輯**之前**，插入以下程式碼：

```c
void  OS_Sched (void)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr = 0u;
#endif

    OS_ENTER_CRITICAL();

    /* ===== 新增：RM 排程邏輯 ===== */
    if (OSIntNesting == 0u && OSLockNesting == 0u) {
        OS_TCB  *ptcb;
        OS_TCB  *best_tcb  = (OS_TCB *)0;
        INT32U   min_period = 0xFFFFFFFFu;
        INT8U    prio;
        INT8U    next_prio  = 1u;   /* 從優先權 1 開始重新分配 */

        /* 第一輪：找出所有 ready task，按 period 排序後重新分配優先權 */
        /* 簡化作法：找到 period 最小的 ready task，給它優先權 1 */

        for (prio = 0u; prio < OS_LOWEST_PRIO; prio++) {
            ptcb = OSTCBPrioTbl[prio];
            if (ptcb != (OS_TCB *)0 && ptcb != OS_TCB_RESERVED) {
                if ((ptcb->OSTCBStat & OS_STAT_SUSPEND) == OS_STAT_RDY) {
                    if (ptcb->OSTCBPeriod < min_period) {
                        min_period = ptcb->OSTCBPeriod;
                        best_tcb   = ptcb;
                    }
                }
            }
        }

        /* 把 period 最短的 task 優先權設為 1（最高）*/
        if (best_tcb != (OS_TCB *)0 && best_tcb->OSTCBPrio != 1u) {
            OS_EXIT_CRITICAL();
            OSTaskChangePrio(best_tcb->OSTCBPrio, 1u);
            OS_ENTER_CRITICAL();
        }
    }
    /* ===== 新增結束 ===== */

    /* 以下是原有排程邏輯，不要修改 */
    if (OSIntNesting == 0u) {
        if (OSLockNesting == 0u) {
            OS_SchedNew();
            OSTCBHighRdy = OSTCBPrioTbl[OSPrioHighRdy];
            if (OSPrioHighRdy != OSPrioCur) {
#if OS_TASK_PROFILE_EN > 0u
                OSTCBHighRdy->OSTCBCtxSwCtr++;
#endif
                OSCtxSwCtr++;
                OS_TASK_SW();
            }
        }
    }
    OS_EXIT_CRITICAL();
}
```

### OSTaskChangePrio() 用法

```c
INT8U OSTaskChangePrio(INT8U oldprio, INT8U newprio);
/*
 * oldprio：task 目前的優先權數字（可由 ptcb->OSTCBPrio 取得）
 * newprio：要改成的優先權數字（越小越高）
 * 回傳值：OS_ERR_NONE 表示成功
 */
```

### 驗證方式

用以下 taskset.txt 跑程式，驗證週期 7 的 task 最先執行：
```
4
1 12
1 7
2 19
3 20
```
預期：時間 0~1 由週期 7 的 task 執行，接著排週期 12 的 task。

---

## 四、組員 B 的工作（EDF 排程器）

> **Branch 名稱：** `feature/edf`  
> **從 main 開 branch（等基礎架構 merge 後才開始）**

### 目標

修改 `OS_Sched()`，讓每次排程前先把「絕對 deadline 最小的 ready task」的優先權設為最高。Deadline 會在每個週期結束時更新。

### 需要動的檔案

- `os_core.c`（修改 `OS_Sched()`）

### 實作說明

邏輯與 RM 完全相同，只是把比較的欄位從 `OSTCBPeriod` 改成 `OSTCBDeadline`：

```c
/* ===== 新增：EDF 排程邏輯 ===== */
if (OSIntNesting == 0u && OSLockNesting == 0u) {
    OS_TCB  *ptcb;
    OS_TCB  *best_tcb    = (OS_TCB *)0;
    INT32U   min_deadline = 0xFFFFFFFFu;
    INT8U    prio;

    for (prio = 0u; prio < OS_LOWEST_PRIO; prio++) {
        ptcb = OSTCBPrioTbl[prio];
        if (ptcb != (OS_TCB *)0 && ptcb != OS_TCB_RESERVED) {
            if ((ptcb->OSTCBStat & OS_STAT_SUSPEND) == OS_STAT_RDY) {
                if (ptcb->OSTCBDeadline < min_deadline) {
                    min_deadline = ptcb->OSTCBDeadline;
                    best_tcb     = ptcb;
                }
            }
        }
    }

    if (best_tcb != (OS_TCB *)0 && best_tcb->OSTCBPrio != 1u) {
        OS_EXIT_CRITICAL();
        OSTaskChangePrio(best_tcb->OSTCBPrio, 1u);
        OS_ENTER_CRITICAL();
    }
}
/* ===== 新增結束 ===== */
```

### 關於 Deadline 的更新

基礎架構的 task 骨架已經包含：
```c
OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;
```
這行在 `OSTimeDly()` 之前執行，**不需要你額外處理**，確認它在骨架裡有出現即可。

### 驗證方式

用以下 taskset.txt 驗證（注意 deadline 最近的 task 要最先執行）：
```
5
1 18
1 17
2 16
1 20
1 6
```

---

## 五、組員 C 的工作（輸出格式 + PCP）

> **Branch 名稱：** `feature/output-pcp`  
> **Part 1（輸出）可以和 RM/EDF 同步進行；Part 2（PCP）需要等 RM 完成後再接上**

### Part 1：輸出格式（Context Switch Log）

**目標：** 讓程式輸出每個時間點正在執行的 task 和 context switch 發生的時刻。

**在 `OS_Sched()` 中加入 log（在原有排程邏輯的 context switch 判斷處）：**

```c
/* 在 OS_TASK_SW() 之前加入 */
if (OSPrioHighRdy != OSPrioCur) {
    printf("[t=%lu] Context Switch: Task %d (prio=%d) -> Task %d (prio=%d)\n",
           (unsigned long)OSTimeGet(),
           (int)OSTCBCur->OSTCBId,
           (int)OSTCBCur->OSTCBPrio,
           (int)OSTCBHighRdy->OSTCBId,
           (int)OSTCBHighRdy->OSTCBPrio);
}
```

**預期輸出格式：**
```
[t=0]  Context Switch: Idle(prio=63) -> Task2(prio=1)
[t=1]  Context Switch: Task2(prio=1) -> Task1(prio=2)
[t=8]  Context Switch: Task1(prio=2) -> Task2(prio=1)
...
```

---

### Part 2：PCP（Priority Ceiling Protocol）實作在 RM 上（+30%）

#### PCP 概念

PCP 解決的問題：多個 task 共用資源（semaphore）時，可能發生「priority inversion（優先權反轉）」，讓低優先權 task 卡住高優先權 task。

**PCP 規則：**
1. 每個 semaphore 設定一個 **priority ceiling** = 所有可能使用它的 task 中，優先權最高（數字最小）者的優先權數字
2. Task T 要 lock semaphore S 時，必須滿足：T 的優先權數字 < 所有「目前已被其他 task 鎖住的 semaphore」的 ceiling
3. 若不滿足，T 阻塞，且持有最高 ceiling semaphore 的 task 繼承 T 的優先權

#### 步驟 1：在 OS_EVENT 加入 ceiling 欄位

**檔案：** `ucos_ii.h`，找到 `OS_EVENT` struct：

```c
typedef struct os_event {
    INT8U    OSEventType;      /* 原有欄位 */
    void    *OSEventPtr;       /* 原有欄位：指向持有此 semaphore 的 task TCB */
    INT16U   OSEventCnt;       /* 原有欄位 */
    OS_PRIO  OSEventGrp;       /* 原有欄位 */
    OS_PRIO  OSEventTbl[OS_EVENT_TBL_SIZE]; /* 原有欄位 */

    /* 新增 */
    INT8U    OSEventCeiling;   /* PCP priority ceiling：使用此 semaphore 的 task 中優先權最高者的優先權數字 */
} OS_EVENT;
```

#### 步驟 2：提供設定 ceiling 的介面

在 `main.c` 建立 semaphore 後，手動設定 ceiling：

```c
OS_EVENT *MySem;
MySem = OSSemCreate(1);          /* 建立 semaphore，初始值 1（未被鎖） */
MySem->OSEventCeiling = 1;       /* ceiling = 最高優先權 task 的優先權數字 */
```

#### 步驟 3：修改 OSSemPend()

**檔案：** `os_sem.c`

在原有 pend 邏輯之前插入 PCP 檢查：

```c
void  OSSemPend (OS_EVENT  *pevent,
                 INT16U     timeout,
                 INT8U     *perr)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr = 0u;
#endif

    OS_ENTER_CRITICAL();

    /* ===== 新增：PCP 檢查 ===== */
    if (pevent->OSEventCnt > 0u) {  /* semaphore 目前未被鎖 */
        /* 掃描所有被鎖住的 semaphore，找出最小（最高優先權）的 ceiling */
        INT8U sys_ceiling = OS_LOWEST_PRIO;
        /* 注意：需要維護一個「目前被鎖住的 semaphore 清單」才能掃描 */
        /* 簡化作法：直接用此 semaphore 的 ceiling 做判斷 */

        if (OSTCBCur->OSTCBPrio >= pevent->OSEventCeiling) {
            /* 優先權不夠高，不能 lock，阻塞 */
            OS_TCB *holder = (OS_TCB *)pevent->OSEventPtr;
            if (holder != (OS_TCB *)0) {
                /* 持有者繼承當前 task 的優先權 */
                if (holder->OSTCBPrio > OSTCBCur->OSTCBPrio) {
                    OS_EXIT_CRITICAL();
                    OSTaskChangePrio(holder->OSTCBPrio, OSTCBCur->OSTCBPrio);
                    OS_ENTER_CRITICAL();
                }
            }
            /* 讓當前 task 進入等待 */
            OSTCBCur->OSTCBStat    |= OS_STAT_SEM;
            OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;
            OSTCBCur->OSTCBDly      = timeout;
            OS_EventTaskWait(pevent);
            OS_EXIT_CRITICAL();
            OS_Sched();
            /* ... 後續處理 ... */
            *perr = OS_ERR_NONE;
            return;
        } else {
            /* 可以 lock，記錄持有者 */
            pevent->OSEventCnt--;
            pevent->OSEventPtr = (void *)OSTCBCur;  /* 記錄持有此 semaphore 的 task */
            OS_EXIT_CRITICAL();
            *perr = OS_ERR_NONE;
            return;
        }
    }
    /* ===== 新增結束 ===== */

    /* 以下原有邏輯：semaphore 已被鎖，進入等待 */
    /* ... */
}
```

#### 步驟 4：修改 OSSemPost()

**檔案：** `os_sem.c`

在釋放 semaphore 後，恢復繼承的優先權：

```c
INT8U  OSSemPost (OS_EVENT *pevent)
{
    /* ... 原有 post 邏輯 ... */

    /* ===== 新增：PCP 優先權恢復 ===== */
    /* 若目前 task 的優先權是繼承來的（與原始優先權不同），恢復原始值 */
    if (OSTCBCur->OSTCBPrio != OSTCBCur->OSTCBPrioOrg) {
        OSTaskChangePrio(OSTCBCur->OSTCBPrio, OSTCBCur->OSTCBPrioOrg);
    }
    pevent->OSEventPtr = (void *)0;  /* 清除持有者記錄 */
    /* ===== 新增結束 ===== */

    /* 觸發重新排程 */
    OS_Sched();
    return (OS_ERR_NONE);
}
```

#### 需要基礎架構配合的部分

請基礎架構的人確認 `OS_TCB` 裡有加入：
```c
INT8U  OSTCBPrioOrg;   /* task 原始優先權，PCP 恢復用 */
```
（基礎架構的工作說明裡已包含這個欄位）

---

## 六、Git 分支策略

```
main
├── feature/rm           ← 組員 A（等基礎架構 merge 到 main 後才開）
├── feature/edf          ← 組員 B（等基礎架構 merge 到 main 後才開）
└── feature/output-pcp   ← 組員 C（可與 rm/edf 同時進行）
```

**流程：**
1. 基礎架構完成 → merge 到 main
2. 各組員從 main 開 branch 開始工作
3. 各自完成後發 PR，由基礎架構的人 review 並 merge
4. 最後整合測試

**Branch 指令：**
```bash
git checkout main
git pull
git checkout -b feature/rm    # 組員 A
```

---

## 七、報告大綱（截止 2026/06/08）

格式：4頁 A4、12pt、含所有人姓名與學號

建議章節：
1. **系統架構**：修改了哪些檔案、各模組關係圖
2. **RM 實作**：流程圖 + OS_Sched() 修改說明
3. **EDF 實作**：流程圖 + deadline 更新機制說明
4. **PCP 實作**（加分題）：PCP 規則說明 + 實作細節
5. **測試結果**：輸出截圖 + 是否符合預期
