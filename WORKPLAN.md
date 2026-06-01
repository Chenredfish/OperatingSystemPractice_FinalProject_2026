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
基礎架構的人會加入以下欄位，**其他人在程式中可以直接用**：

| 欄位名稱 | 型別 | 用途 |
|----------|------|------|
| `OSTCBPeriod` | INT32U | task 的週期（ticks） |
| `OSTCBExecTime` | INT32U | task 的執行時間（ticks） |
| `OSTCBDeadline` | INT32U | task 的絕對截止時間（EDF 使用） |
| `OSTCBPrioOrg` | INT8U | task 的原始優先權（PCP 恢復用） |

`OSTCBCur` 是 uC/OS II 內建的全域指標，永遠指向**目前正在執行的 task 的 TCB**。

### 1-5 RM vs EDF 核心差異

| | RM（Rate Monotonic） | EDF（Earliest Deadline First） |
|--|----------------------|-------------------------------|
| 排程依據 | **週期**（靜態，不會變） | **絕對截止時間**（動態，每個週期更新） |
| 誰的優先權最高 | `OSTCBPeriod` 最小的 task | `OSTCBDeadline` 最小的 task |
| 優先權幾時重新計算 | 每次呼叫 `OS_Sched()` | 每次呼叫 `OS_Sched()` |

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

### 工作清單

- [ ] 修改 `ucos_ii.h`：在 OS_TCB 加入新欄位
- [ ] 修改 `os_task.c`：更新 `OSTaskCreateExt()` 簽名與初始化
- [ ] 修改 `main.c`：讀取 taskset.txt 並建立 tasks
- [ ] 定義共用的 task 骨架函數

### 需要動的地方

**`ucos_ii.h`**  
找到 `OS_TCB` struct，加入上方表格列出的四個新欄位。

**`os_task.c` — `OSTaskCreateExt()`**  
原本這個函數接受優先權、stack 大小等參數來建立 task。  
需要額外加入 `period` 和 `exec_time` 兩個參數，並在建立 TCB 後把值寫進對應欄位。  
初始 `OSTCBDeadline` 建議設為第一個週期結束時（即等於 period）。  
注意：`ucos_ii.h` 中也有這個函數的宣告，記得同步修改。

**`main.c`**  
程式啟動後要讀取 `taskset.txt`，根據裡面的數量和參數動態建立對應的 tasks，最後呼叫 `OSStart()` 開始執行。

**Task 骨架函數**  
每個 task 都執行無窮迴圈。每次迴圈代表一個執行週期，流程是：
1. 記錄開始時間（`OSTimeGet()`）
2. 模擬執行直到 `OSTCBExecTime` 時間到
3. 更新下一個絕對 deadline（`OSTCBDeadline += OSTCBPeriod`）
4. 計算剩餘等待時間後呼叫 `OSTimeDly()` 休眠到下一個週期

---

## 三、組員 A 的工作（RM 排程器）

> **Branch 名稱：** `feature/rm`  
> **等基礎架構 merge 進 main 後，從 main 開 branch**

### 目標

修改 `os_core.c` 中的 `OS_Sched()`，讓系統在每次做排程決定之前，先根據 RM 規則重新計算誰應該有最高優先權。

### RM 規則

> 週期（`OSTCBPeriod`）越短的 task，優先權越高（優先權數字越小）。

### 需要理解的事

- `OS_Sched()` 內部如何找到下一個要執行的 task（閱讀原始碼）
- `OSTCBPrioTbl[]` 是什麼：一個以優先權數字為 index 的陣列，存放每個優先權對應的 TCB 指標
- `OSTCBStat` 欄位如何判斷一個 task 是否在 ready 狀態
- `OSTaskChangePrio(oldprio, newprio)` 的使用方式和限制（同一個優先權數字不能同時有兩個 task）

### 要做的事

在 `OS_Sched()` 的排程邏輯**之前**，插入一段程式：
1. 掃描所有存在的 TCB
2. 找出所有處於 ready 狀態的 task
3. 依照 `OSTCBPeriod` 由小到大排序
4. 用 `OSTaskChangePrio()` 重新分配優先權數字（period 最小 → 優先權數字最小）
5. 讓 uC/OS II 原有的排程邏輯繼續執行（它會自然選到優先權最高的那個）

### 注意事項

- 優先權衝突：重新分配時，必須先把舊優先權讓出來再指定新的，否則 `OSTaskChangePrio()` 會失敗
- 不要修改 `OS_IDLE_PRIO`（idle task 的優先權，通常是最低的那個）

### 驗證

用 Input Example 1 跑程式，確認週期 7 的 task 確實最先執行。

---

## 四、組員 B 的工作（EDF 排程器）

> **Branch 名稱：** `feature/edf`  
> **等基礎架構 merge 進 main 後，從 main 開 branch**

### 目標

修改 `os_core.c` 中的 `OS_Sched()`，讓系統每次排程時根據 EDF 規則選出下一個執行的 task。

### EDF 規則

> 絕對 deadline（`OSTCBDeadline`）越小（越早到期）的 task，優先權越高。

### RM vs EDF 的關鍵差異

EDF 的 `OS_Sched()` 邏輯結構和 RM **幾乎相同**，差別在於：
- RM 比較 `OSTCBPeriod`
- EDF 比較 `OSTCBDeadline`

但 EDF 的難點在於 deadline 是**動態變化**的——每次 task 完成一個週期後 deadline 要往前推一個 period。這個更新已經在 task 骨架裡（基礎架構的人完成），你需要確認它的時機是否正確。

### 需要理解的事

- `OSTCBDeadline` 代表的是**絕對時間**（從系統啟動到截止的 ticks），不是剩餘時間
- Deadline 更新必須在 `OSTimeDly()` **之前**發生，否則排程器看到的還是舊的 deadline

### 要做的事

1. 確認 task 骨架中 deadline 更新的位置是否正確
2. 在 `OS_Sched()` 插入 EDF 排程邏輯（結構參考 RM，欄位改為 `OSTCBDeadline`）
3. 同樣需要處理優先權衝突問題

### 驗證

用 Input Example 2 驗證輸出序列是否符合 EDF（deadline 最近的先跑）。

---

## 五、組員 C 的工作（輸出格式 + PCP）

> **Branch 名稱：** `feature/output-pcp`

### Part 1：輸出格式（Context Switch Log）

**目標：** 讓程式在每次 context switch 時，輸出目前系統時間和切換前後的 task 資訊。

**要動的地方：** `os_core.c` 的 `OS_Sched()`

找到判斷是否發生 context switch 的位置（當「下一個要執行的 task」和「目前執行中的 task」不同時），在切換**發生前**印出：
- 目前系統時間（`OSTimeGet()`）
- 切換前的 task ID
- 切換後的 task ID

輸出格式自訂，但要能清楚看出執行序列和切換時刻。

---

### Part 2：PCP（Priority Ceiling Protocol）實作在 RM 上（+30%）

#### PCP 概念

PCP 解決多個 task 共用資源（semaphore）時可能發生的 **priority inversion（優先權反轉）** 問題。

**核心規則：**
1. 每個 semaphore 有一個 **priority ceiling** = 所有可能使用它的 task 中，優先權最高者的優先權數字
2. Task T 要 lock 一個 semaphore 時，必須滿足：T 的優先權數字 **小於**（高於）所有「目前已被其他 task 鎖住的 semaphore」的 ceiling
3. 若不滿足，T 阻塞，且持有最高 ceiling semaphore 的 task 必須**繼承** T 的優先權

**要理解的問題：**
- 為什麼 priority inversion 有害？（高優先權 task 被低優先權 task 間接卡住）
- PCP 和 PIP 的差別是什麼？（PCP 是預防性的，PIP 是被動繼承）

#### 需要動的地方

**`ucos_ii.h` — `OS_EVENT` struct**  
加入 `OSEventCeiling` 欄位，用來儲存這個 semaphore 的 priority ceiling 值。

**`os_sem.c` — `OSSemPend()`**  
在 task 嘗試 lock semaphore 之前，加入 PCP 的條件判斷：
- 若 task 的優先權不符合條件，阻塞並觸發優先權繼承
- 若符合條件，正常 lock 並記錄持有者

**`os_sem.c` — `OSSemPost()`**  
在 task 釋放 semaphore 後：
- 恢復被繼承走的優先權（`OSTCBPrioOrg`）
- 清除持有者記錄
- 觸發重新排程

#### 需要基礎架構配合的部分

`OS_TCB` 裡的 `OSTCBPrioOrg` 欄位（基礎架構已加入），用於記錄 task 的原始優先權，讓 PCP 在釋放資源後能恢復回正確的值。

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

```bash
git checkout main
git pull
git checkout -b feature/rm    # 組員 A 執行這行
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
