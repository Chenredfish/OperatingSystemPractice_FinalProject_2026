# 作業系統期末專題 — 工作分配說明

> 長庚大學 作業系統實習 Final Project  
> 截止日：2026/06/08 23:00 | Demo：2026/06/11

**每個人只需閱讀自己對應的章節，其他章節可選擇性閱讀。**

| 章節 | 負責人 | 工作摘要 |
|------|--------|----------|
| [一](#一組員-a-的工作rm-排程器) | 組員 A | RM 排程器：完成 EX_RM/TEST.C |
| [二](#二組員-b-的工作edf-排程器) | 組員 B | EDF 排程器：完成 EX_EDF/TEST.C |
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
2. **對兩個陣列做 bubble sort，依 `TaskPeriod[]` 升序排列**（注意：排序時要同步移動 `TaskExecTime[]`）
3. 用迴圈建立 tasks，`prio = i + 1`（i 從 0 開始，所以 priority 是 1, 2, 3...）
   - `OSTaskCreateExt(PeriodicTask, (void*)(INT32U)(i+1), &TaskStk[i][TASK_STK_SIZE-1], prio, prio, &TaskStk[i][0], TASK_STK_SIZE, (void*)0, OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR, TaskPeriod[i], TaskExecTime[i])`
4. 每個 task 建立後，用 `PC_DispStr` 顯示 task 資訊（row 用 5+i）

---

### PeriodicTask() 要做什麼

每次迴圈代表一個執行週期：

1. 記錄開始時間：`start_tick = OSTimeGet()`
2. 忙碌等待，直到經過 `OSTCBCur->OSTCBExecTime` ticks：
   ```c
   while ((OSTimeGet() - start_tick) < OSTCBCur->OSTCBExecTime) { ; }
   ```
   這段期間，若有更高優先權的 task ready，排程器會自動搶占（preempt）此 task
3. 計算實際經過時間：`elapsed = OSTimeGet() - start_tick`
4. 用 `PC_DispStr` 輸出此 task 在哪個 tick 執行了多久（row 用 13+task_id）
5. 計算剩餘等待時間並休眠：
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

在以下檔案完成兩個函數：

```
SOFTWARE/uCOS-II/EX_EDF/BC45/SOURCE/TEST.C
```

- `TaskStartCreateTasks()`：讀取 taskset.txt，建立 tasks
- `PeriodicTask()`：每個 task 的執行主體，**包含 deadline 更新邏輯**

EDF 的排程邏輯（在 `OS_Sched()` 裡找最小 deadline 的 task 給它最高優先權）**已由基礎架構實作完成**，`OS_CFG.H` 裡也已定義 `SCHED_EDF`。你只需要讓 tasks 在每個週期結束後正確更新自己的 deadline。

---

### EDF 概念

Earliest Deadline First 排程規則：**絕對 deadline 最近（最小）的 task，優先權最高**。
與 RM 不同，EDF 的優先權是**動態的**——每個週期後 deadline 往後推一個 period，排程器根據新 deadline 重新決定誰最緊急。

uC/OS II 原本用優先權數字決定誰先跑，EDF 的做法是：每次排程時找出 deadline 最小的 ready task，用 `OSTaskChangePrio()` 把它的優先權改成 1（最高），讓原本的排程器自然選到它。

---

### taskset.txt 格式（同 EX_RM）

```
4
1 12
1 7
2 19
3 20
```

---

### 你需要用到的 TCB 欄位

| 欄位 | 型別 | 說明 |
|------|------|------|
| `OSTCBPeriod` | INT32U | task 的週期（ticks） |
| `OSTCBExecTime` | INT32U | task 的執行時間（ticks） |
| `OSTCBDeadline` | INT32U | task 的**絕對截止時間**（從系統啟動算的 ticks），初始值 = period，每個週期後 += period |

`OSTCBCur` 是 uC/OS II 內建的全域指標，永遠指向目前執行中的 task 的 TCB。

---

### 你需要用到的函數

（同組員 A：`OSTaskCreateExt` / `OSTimeGet` / `OSTimeDly` / `PC_DispStr`，用法相同）

---

### TaskStartCreateTasks() 要做什麼

EDF **不需要排序**，OS_Sched() 會動態處理順序。直接建立 tasks，分配優先權 `BASE_PRIO + i`（BASE_PRIO = 10，所以是 10, 11, 12...）。

1. `fopen("taskset.txt", "r")` 讀取 TaskCount 和各 task 的 exec_time / period
2. 直接建立 tasks，`prio = BASE_PRIO + i`
   - 參數格式同組員 A，最後兩個傳 `TaskPeriod[i]` 和 `TaskExecTime[i]`

---

### PeriodicTask() 要做什麼

步驟與 RM 幾乎相同，**關鍵差異是第 4 步**，必須在 `OSTimeDly()` 之前更新 deadline：

1. `start_tick = OSTimeGet()`
2. 忙碌等待 `OSTCBCur->OSTCBExecTime` ticks
3. `elapsed = OSTimeGet() - start_tick`
4. 用 `PC_DispStr` 輸出此 task 的執行資訊（建議顯示 `OSTCBDeadline` 方便驗證）
5. **更新 deadline（必須在 OSTimeDly 之前）：**
   ```c
   OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;
   ```
6. 計算剩餘等待時間並休眠：`OSTimeDly((INT16U)(OSTCBCur->OSTCBPeriod - elapsed))`

**為什麼 deadline 更新必須在 OSTimeDly() 之前？**  
task 呼叫 `OSTimeDly()` 後進入 delayed 狀態，醒來時排程器立即重新決定誰先跑。排程器讀的是 `OSTCBDeadline`，如果這時還沒更新，排程器看到的是上一個週期的舊 deadline，會做出錯誤決定。

---

### 編譯與執行

1. 把整個 `SOFTWARE` 資料夾複製到虛擬機 `C:\`（覆蓋原本的）
2. 在虛擬機進入 `C:\SOFTWARE\uCOS-II\EX_EDF\BC45\TEST\`
3. 執行 `CLEAN.BAT`（清除舊的編譯產物，**每次重新複製後都要執行**）
4. 執行 `MAKETEST.BAT` 編譯
5. 執行產生的 `TEST.EXE`，按 ESC 離開

> `taskset.txt` 已放在 TEST\ 資料夾內，格式同 EX_RM。

> **注意**：不要把虛擬機裡的 TEST.EXE 複製回主機的 SOFTWARE 資料夾。

### 驗證

觀察輸出，確認每次執行的都是當下 deadline 最小（最緊急）的 task。

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
| `OSEventOwner` | `OS_TCB *` | 目前持有此 semaphore 的 task |

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
OS_TCB   *OSEventOwner;    /* PCP: TCB of task currently holding this semaphore */
```

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
