# 作業系統期末專題 — 工作分配說明

> 長庚大學 作業系統實習 Final Project  
> 截止日：2026/06/08 23:00 | Demo：2026/06/11

**每個人只需閱讀自己對應的章節，其他章節可選擇性閱讀。**

| 章節 | 負責人 | 工作摘要 |
|------|--------|----------|
| [一](#一組員-a-的工作rm-排程器) | 組員 A | RM 排程器：完成 EX_RM/TEST.C |
| [二](#二組員-b-的工作edf-排程器) | 組員 B | EDF 排程器：實作 OS_CORE.C 排程邏輯 |
| [三](#三組員-c-的工作pcp-加分題30) | 組員 C | PCP 加分題：修改 OS_SEM.C + uCOS_II.H |
| [四](#四報告大綱) | 全體 | 報告大綱 |

---

## 一、組員 A 的工作：RM 排程器

### 你的任務

在以下檔案完成兩個函數：

```
SOFTWARE/uCOS-II/EX_RM/BC45/SOURCE/TEST.C
```

- `TaskStartCreateTasks()`：讀取 taskset.txt，依 RM 規則排序並建立 tasks
- `PeriodicTask()`：每個 task 的執行主體

OS_Sched() 和其他核心檔案**不需要動**。RM 的靜態優先權在建立 task 時就決定好，uC/OS II 原有排程器自然會選最高優先權的 ready task 執行。

---

### RM 概念

Rate Monotonic 排程規則：**週期越短的 task，優先權越高**。
這個優先權在系統啟動時就固定，不會因執行狀況而改變（靜態優先權）。

uC/OS II 的優先權機制：**數字越小 = 優先權越高**，排程器永遠挑數字最小的 ready task 來跑。

所以 RM 的做法是：把任務按 period 由小到大排序，分配優先權 1, 2, 3...（period 最短的拿到 1，天然最優先）。

---

### taskset.txt 格式

```
4          ← task 數量
1 12       ← task 1：執行時間=1 ticks, 週期=12 ticks
1 7        ← task 2：執行時間=1 ticks, 週期=7 ticks
2 19       ← task 3：執行時間=2 ticks, 週期=19 ticks
3 20       ← task 4：執行時間=3 ticks, 週期=20 ticks
```

格式：每行兩個數字，依序是 `exec_time` 和 `period`。

---

### 你需要用到的 TCB 欄位

基礎架構已在 OS_TCB 加入以下欄位，透過 `OSTCBCur->欄位名稱` 存取目前執行中 task 的值：

| 欄位 | 型別 | 說明 |
|------|------|------|
| `OSTCBPeriod` | INT32U | task 的週期（ticks） |
| `OSTCBExecTime` | INT32U | task 的執行時間（ticks） |

`OSTCBCur` 是 uC/OS II 內建的全域指標，永遠指向目前執行中的 task 的 TCB。

---

### 你需要用到的函數

**`OSTaskCreateExt(func, pdata, stk_top, prio, id, stk_bot, stk_size, ext, opt, period, exec_time)`**  
建立 task。最後兩個參數是基礎架構新增的 `period` 和 `exec_time`。  
其中 `prio` 和 `id` 填同一個值即可，`stk_top` 是 `&TaskStk[i][TASK_STK_SIZE-1]`，`stk_bot` 是 `&TaskStk[i][0]`。

**`OSTimeGet()`**  
回傳目前系統時間（ticks），型別 INT32U。

**`OSTimeDly(ticks)`**  
讓目前 task 暫停指定 ticks 後再恢復，用於等待下一個週期。

**`PC_DispStr(col, row, str, attr)`**  
在螢幕指定位置顯示字串。

---

### TaskStartCreateTasks() 要做什麼

1. `fopen("taskset.txt", "r")` 讀取 `TaskCount`，再用迴圈讀取每個 task 的 `TaskExecTime[i]` 和 `TaskPeriod[i]`
   讀完後立刻換算為 ticks：
   ```c
   TaskExecTime[i] *= OS_TICKS_PER_SEC;
   TaskPeriod[i]   *= OS_TICKS_PER_SEC;
   ```
2. **對兩個陣列做 bubble sort，依 `TaskPeriod[]` 升序排列**（注意：排序時要同步移動 `TaskExecTime[]`）
3. 用迴圈建立 tasks，`prio = i + 1`（i 從 0 開始，所以 priority 是 1, 2, 3...）
   - `OSTaskCreateExt(PeriodicTask, (void*)(INT32U)(i+1), &TaskStk[i][TASK_STK_SIZE-1], prio, prio, &TaskStk[i][0], TASK_STK_SIZE, (void*)0, OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR, TaskPeriod[i], TaskExecTime[i])`
4. 每個 task 建立後，用 `PC_DispStr` 顯示 task 資訊（row 用 5+i），**顯示時除以 `OS_TICKS_PER_SEC` 換算回秒**：
   ```c
   sprintf(s, "Task%d: exec=%lds period=%lds prio=%d",
           i+1, TaskExecTime[i]/OS_TICKS_PER_SEC,
           TaskPeriod[i]/OS_TICKS_PER_SEC, prio);
   ```

---

### PeriodicTask() 要做什麼

每次迴圈代表一個執行週期。需要宣告 `end_tick` 和 `run_count` 兩個額外變數（`run_count = 0` 在迴圈外初始化）：

1. 記錄開始時間並遞增計數：
   ```c
   start_tick = OSTimeGet();
   run_count++;
   ```
2. **在忙碌等待之前**輸出 context switch 時刻（row 用 `11 + task_id`）：
   ```c
   sprintf(s, "Task%d  start=%4lds  end=----s  period=%4lds  #%3d",
           (int)task_id,
           start_tick / OS_TICKS_PER_SEC,
           OSTCBCur->OSTCBPeriod / OS_TICKS_PER_SEC,
           (int)run_count);
   PC_DispStr(0, row, s, DISP_FGND_WHITE + DISP_BGND_BLACK);
   ```
3. 忙碌等待，直到經過 `OSTCBCur->OSTCBExecTime` ticks：
   ```c
   while ((OSTimeGet() - start_tick) < OSTCBCur->OSTCBExecTime) { ; }
   ```
   這段期間，若有更高優先權的 task ready，排程器會自動搶占（preempt）此 task
4. 記錄結束時間，計算 elapsed：
   ```c
   end_tick = OSTimeGet();
   elapsed  = end_tick - start_tick;
   ```
5. **在忙碌等待之後**輸出完整資訊（填入 end time，覆蓋同一 row）：
   ```c
   sprintf(s, "Task%d  start=%4lds  end=%4lds  period=%4lds  #%3d",
           (int)task_id,
           start_tick / OS_TICKS_PER_SEC,
           end_tick / OS_TICKS_PER_SEC,
           OSTCBCur->OSTCBPeriod / OS_TICKS_PER_SEC,
           (int)run_count);
   PC_DispStr(0, row, s, DISP_FGND_WHITE + DISP_BGND_BLACK);
   ```
6. 計算剩餘等待時間並休眠：
   ```c
   delay_ticks = OSTCBCur->OSTCBPeriod - elapsed;
   if ((INT32S)delay_ticks > 0) { OSTimeDly((INT16U)delay_ticks); }
   ```

`task_id` 從 pdata 取得：`task_id = (INT8U)(INT32U)pdata`

---

### 編譯與執行

1. 把整個 `SOFTWARE` 資料夾複製到虛擬機 `C:\`（覆蓋原本的）
2. 在虛擬機進入 `C:\SOFTWARE\uCOS-II\EX_RM\BC45\TEST\`
3. 執行 `CLEAN.BAT`（清除舊的編譯產物，**每次重新複製後都要執行**）
4. 執行 `MAKETEST.BAT` 編譯
5. 執行產生的 `TEST.EXE`，按 ESC 離開

> `taskset.txt` 已放在 TEST\ 資料夾內，格式為 3 個 task：`(exec=1, period=4)`、`(exec=1, period=6)`、`(exec=1, period=12)`，可自行修改。

> **注意**：不要把虛擬機裡的 TEST.EXE 複製回主機的 SOFTWARE 資料夾，否則下次複製時會蓋掉重新編譯的版本。

### 驗證

觀察輸出，確認 period=4 的 task 最先且最頻繁執行，period=6 次之，period=12 最少。

---

## 二、組員 B 的工作：EDF 排程器

### 你的任務

在以下檔案實作 EDF 排程邏輯：

```
SOFTWARE/uCOS-II/SOURCE/OS_CORE.C
```

找到 `OS_Sched()` 函數中的 `#ifdef SCHED_EDF ... #endif` 區塊，填入 EDF 動態排程的程式碼。

`EX_EDF/BC45/SOURCE/TEST.C` 已由組長完成，不需要動。

---

### EDF 概念

Earliest Deadline First 排程規則：**絕對 deadline 最近（最小）的 task，優先權最高**。

uC/OS II 原本用固定優先權決定誰先跑。EDF 的做法是：每次排程時找出 deadline 最小的 ready task，用 `OSTaskChangePrio()` 把它的優先權改成 1，讓原本排程器自然選到它。

每個 task 執行完一個週期後，會自己把 `OSTCBDeadline += OSTCBPeriod`（TEST.C 已完成）。OS_Sched() 讀的就是這個最新的 deadline。

---

### 你需要用到的欄位與變數

| 欄位／變數 | 說明 |
|------------|------|
| `OSTCBList` | TCB 鏈結串列頭，從這裡開始走訪 |
| `ptcb->OSTCBNext` | 下一個 TCB（走到 NULL 結束） |
| `OSTCBStat` | task 狀態，`OS_STAT_RDY` = 就緒 |
| `OSTCBPrio` | task 目前優先權（數字小 = 優先權高） |
| `OSTCBPrioOrg` | task 原始優先權（被動態改過時記住原值） |
| `OSTCBPeriod` | task 週期，`== 0` 代表系統 task，要跳過 |
| `OSTCBDeadline` | task 的絕對截止時間，數字越小越緊急 |
| `OSTCBPrioTbl[1]` | 目前佔著優先權 1 的 TCB 指標 |
| `OS_IDLE_PRIO` / `OS_STAT_PRIO` | 系統 task 的優先權，要跳過 |

---

### 要實作什麼

在 `OS_Sched()` 的 `#ifdef SCHED_EDF` 區塊裡，填入：

1. 宣告 `OS_TCB *ptcb`、`OS_TCB *best = (OS_TCB *)0`、`INT32U min_dl = 0xFFFFFFFFl`
2. 從 `OSTCBList` 開始，沿 `OSTCBNext` 走完整個 TCB 鏈結串列
3. 每個 TCB 判斷（全部符合才列入候選）：
   - `OSTCBStat == OS_STAT_RDY`
   - `OSTCBPrio != OS_IDLE_PRIO` 且 `!= OS_STAT_PRIO`
   - `OSTCBPeriod > 0`
   - 若 `OSTCBDeadline < min_dl`，更新 best
4. 找到 best 後，若 `best->OSTCBPrio != 1`：
   - 先把目前佔著 priority 1 的 task 還原（若 `OSTCBPrioTbl[1]` 有值）：
     ```c
     OS_TCB *cur_p1 = OSTCBPrioTbl[1];
     OS_EXIT_CRITICAL();
     OSTaskChangePrio(1, cur_p1->OSTCBPrioOrg);
     OS_ENTER_CRITICAL();
     ```
   - 再把 best 的優先權改為 1：
     ```c
     OS_EXIT_CRITICAL();
     OSTaskChangePrio(best->OSTCBPrio, 1);
     OS_ENTER_CRITICAL();
     ```

> **注意**：`OSTaskChangePrio()` 不能在 critical section 內呼叫，呼叫前後要配對 `OS_EXIT_CRITICAL()` / `OS_ENTER_CRITICAL()`。

---

### 編譯與執行

1. 把整個 `SOFTWARE` 資料夾複製到虛擬機 `C:\`（覆蓋原本的）
2. 在虛擬機進入 `C:\SOFTWARE\uCOS-II\EX_EDF\BC45\TEST\`
3. 執行 `CLEAN.BAT`（清除舊的編譯產物，**每次重新複製後都要執行**）
4. 執行 `MAKETEST.BAT` 編譯
5. 執行產生的 `TEST.EXE`，按 ESC 離開

> `taskset.txt` 已放在 TEST\ 資料夾內，格式為 3 個 task：`(exec=1, period=4)`、`(exec=1, period=6)`、`(exec=1, period=12)`。

> **注意**：不要把虛擬機裡的 TEST.EXE 複製回主機的 SOFTWARE 資料夾。

### 驗證

畫面上每行顯示 task 執行時的 deadline 值。確認每次執行的是當下 deadline 最小的 task。

---

## 三、組員 C 的工作：PCP 加分題（+30%）

### 你的任務

實作 Priority Ceiling Protocol（PCP）。需要修改以下三個地方：

| 檔案 | 要做的事 |
|------|----------|
| `SOFTWARE/uCOS-II/SOURCE/uCOS_II.H` | 在 OS_EVENT struct 加入 `OSEventCeiling` 和 `OSEventOwner` 欄位 |
| `SOFTWARE/uCOS-II/SOURCE/OS_SEM.C` | 修改 `OSSemPend()` 加 PCP 條件判斷與優先權繼承 |
| `SOFTWARE/uCOS-II/SOURCE/OS_SEM.C` | 修改 `OSSemPost()` 加釋放後的優先權恢復 |
| `SOFTWARE/uCOS-II/EX_PCP/BC45/SOURCE/TEST.C` | 設定 `SharedSem->OSEventCeiling` 的值 |

---

### PCP 概念

多個 tasks 共用 semaphore 時，可能發生 **priority inversion（優先權反轉）**：

> 低優先權 task L 持有 semaphore S → 高優先權 task H 來了想要 S → H 被迫等待 L → 中等優先權 task M 搶先執行，把 L 擠出去 → H 等更久

PCP 的解法是「天花板協定」：
1. 每個 semaphore 有一個 **ceiling** = 所有可能使用它的 tasks 中，優先權數字最小的那個（即優先權最高的使用者）
2. Task T 要 lock semaphore 時，必須先確認：T 的優先權數字 **小於**（高於）所有「目前已被其他 task 持有的 semaphore」的 ceiling
3. 若不符合，T 阻塞，持有 semaphore 的 task 必須**繼承** T 的優先權（避免 M 插隊）

uC/OS II 優先權：**數字越小 = 優先權越高**。

以單一 semaphore 的情況，條件簡化為：  
若 semaphore 已被其他 task 持有，且 `OSTCBCur->OSTCBPrio >= pevent->OSEventCeiling`（T 的優先權不高於 ceiling），則 T 阻塞。

---

### 你需要用到的欄位與函數

**OS_TCB 欄位（已存在）**

| 欄位 | 說明 |
|------|------|
| `OSTCBPrio` | task 目前的優先權數字 |
| `OSTCBPrioOrg` | task 原始優先權（基礎架構已加入，用於恢復） |
| `OSTCBStat` | task 狀態，`OS_STAT_RDY` = 就緒 |

**OS_EVENT 欄位（你要加入的）**

| 欄位 | 型別 | 說明 |
|------|------|------|
| `OSEventCeiling` | INT8U | 此 semaphore 的 priority ceiling |
| `OSEventOwner` | `void *` | 目前持有此 semaphore 的 task（使用前 cast 成 `OS_TCB *`） |

**函數**

| 函數 | 說明 |
|------|------|
| `OSTaskChangePrio(oldprio, newprio)` | 動態修改某個 task 的優先權 |
| `OSTCBPrioTbl[prio]` | 以優先權數字為 index，取得對應的 TCB 指標 |
| `OS_Sched()` | 觸發重新排程 |

---

### 步驟 1：在 uCOS_II.H 加入欄位

搜尋 `typedef struct os_event`，在 struct 裡加入：

```c
INT8U     OSEventCeiling;  /* PCP: ceiling priority of this semaphore */
void     *OSEventOwner;    /* PCP: TCB of task currently holding this semaphore (void* to avoid forward-decl issue) */
```

> **注意**：這裡必須用 `void *` 而不是 `OS_TCB *`，因為 OS_EVENT 在檔案中比 OS_TCB 更早定義，此時 `OS_TCB` 這個型別還不存在。在 OS_SEM.C 裡使用時再 cast：`OS_TCB *owner = (OS_TCB *)pevent->OSEventOwner;`

---

### 步驟 2：修改 OSSemPend()

位置：`SOFTWARE/uCOS-II/SOURCE/OS_SEM.C`

找到 `OSSemPend()` 函數。在函數進入後（取得 critical section 之後），原本判斷 semaphore count > 0 就 lock 的地方之前，加入 PCP 條件判斷：

**邏輯流程：**

```
semaphore count > 0（可以 lock）？
  ├─ 是：正常 lock，記錄持有者：pevent->OSEventOwner = OSTCBCur
  └─ 否：semaphore 已被其他 task 持有
         ↓
         PCP 條件：OSTCBCur->OSTCBPrio >= pevent->OSEventCeiling？
           ├─ 是（T 的優先權不夠高）：
           │     找到持有者 pevent->OSEventOwner
           │     若持有者優先權低於 T：OSTaskChangePrio(持有者目前優先權, OSTCBCur->OSTCBPrio)
           │     讓 T 進入 waiting 狀態（參考原本程式碼的阻塞處理）
           │     呼叫 OS_Sched()
           └─ 否（T 優先權夠高）：正常等待，不繼承
```

---

### 步驟 3：修改 OSSemPost()

位置同上。找到 `OSSemPost()` 函數。

在成功 post（遞增 count 或喚醒等待 task）之後，加入優先權恢復：

```
若 pevent->OSEventOwner 不為 NULL
  且 OSEventOwner->OSTCBPrio != OSEventOwner->OSTCBPrioOrg（優先權曾被提升）
    ↓
    OSTaskChangePrio(OSEventOwner->OSTCBPrio, OSEventOwner->OSTCBPrioOrg)
    pevent->OSEventOwner = (OS_TCB *)0
```

---

### 步驟 4：在 TEST.C 設定 ceiling

位置：`SOFTWARE/uCOS-II/EX_PCP/BC45/SOURCE/TEST.C`，`TaskStartCreateTasks()` 函數

找到這行：
```c
SharedSem = OSSemCreate(1);
/* TEAMMATE C: SharedSem->OSEventCeiling = 1; */
```

把 comment 拿掉，改成：
```c
SharedSem = OSSemCreate(1);
SharedSem->OSEventCeiling = 1;   /* = priority of the highest-prio task using this sem */
SharedSem->OSEventOwner   = (OS_TCB *)0;
```

Ceiling = 1 代表使用這個 semaphore 的 tasks 中，最高優先權的是 priority 1。

---

### 編譯與執行

1. 把整個 `SOFTWARE` 資料夾複製到虛擬機 `C:\`（覆蓋原本的）
2. 在虛擬機進入 `C:\SOFTWARE\uCOS-II\EX_PCP\BC45\TEST\`
3. 執行 `CLEAN.BAT`（清除舊的編譯產物，**每次重新複製後都要執行**）
4. 執行 `MAKETEST.BAT` 編譯
5. 執行產生的 `TEST.EXE`，按 ESC 離開

> `taskset.txt` 已放在 TEST\ 資料夾內，格式同 EX_RM。

> **注意**：不要把虛擬機裡的 TEST.EXE 複製回主機的 SOFTWARE 資料夾。

### 驗證

高優先權 task 被低優先權 task 的 semaphore 阻擋時，應觀察到低優先權 task 被提升優先權（輸出會顯示優先權變化），而不是讓中等優先權 task 插隊執行。

---

## 四、報告大綱（截止 2026/06/08 23:00）

格式：4 頁 A4、12pt、含所有人姓名與學號

建議章節：
1. **系統架構**：修改了哪些檔案、各模組關係圖
2. **RM 實作**：流程圖 + 靜態優先權分配說明
3. **EDF 實作**：流程圖 + deadline 更新機制 + OS_Sched() 修改說明
4. **PCP 實作**（加分題）：PCP 規則 + OSSemPend/OSSemPost 修改細節
5. **測試結果**：輸出截圖 + 符合預期的說明
