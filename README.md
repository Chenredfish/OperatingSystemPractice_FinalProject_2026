# uC/OS II 排程器實作 — 專題說明

本專題在 uC/OS II 原始碼基礎上，新增 RM 和 EDF 兩種排程演算法，以及 PCP 互斥鎖機制。

---

## 修改內容

### 1. 新增 TCB 欄位（`SOURCE/uCOS_II.H`）
OS_TCB struct 加入 `OSTCBPeriod`、`OSTCBExecTime`、`OSTCBDeadline`、`OSTCBPrioOrg`。
原因：RM/EDF 需要任務週期與 deadline 資訊；PCP 需要記錄原始優先權。
**By 組長**

### 2. 擴充 OSTaskCreateExt 簽名（`SOURCE/uCOS_II.H`、`SOURCE/OS_TASK.C`）
新增 `period`、`exec_time` 兩個參數，建立任務時一併初始化新 TCB 欄位。
原因：讓 RM/EDF 任務在建立時就設定週期與執行時間。
**By 組長**

### 3. 加入 EDF 排程邏輯（`SOURCE/OS_CORE.C`）
在 `OS_Sched()` 的 `#ifdef SCHED_EDF` 區塊，走訪 TCB 串列找出 deadline 最小的 ready task，用 `OSTaskChangePrio()` 動態給它優先權 1。
原因：原版 uC/OS II 只有固定優先權排程，EDF 需要動態選擇。
**By 組員 B**（TODO 已標記在 OS_CORE.C）

### 4. 建立三個獨立專題資料夾
- `EX_RM/` — RM 排程（Rate Monotonic），組員 A 實作
- `EX_EDF/` — EDF 排程（Earliest Deadline First），組員 B 實作
- `EX_PCP/` — PCP 互斥鎖（Priority Ceiling Protocol），組員 C 實作

每個資料夾各自有完整的 OS 原始碼、OS_CFG.H、TEST.C 骨架、taskset.txt、CLEAN.BAT。
原因：三人分開改 OS_CORE.C，用獨立資料夾避免 git merge 衝突。
**By 組長**

---

## 使用方式（VM 內）

1. 將 `SOFTWARE/` 資料夾整個複製到 VM 的 `C:\`
2. 進入對應專題的 `TEST\` 資料夾（例如 `C:\SOFTWARE\uCOS-II\EX_RM\BC45\TEST\`）
3. 執行 `CLEAN.BAT`（清除舊編譯產物）
4. 執行 `MAKETEST.BAT`（重新編譯）
5. 執行 `TEST.EXE`

> 注意：不要把 VM 裡的 TEST.EXE 複製回主機，否則下次複製時會蓋掉重新編譯的版本。

---

## 檔案說明

| 檔案 | 說明 |
|------|------|
| `WORKPLAN.md` | 各組員實作指南（各看自己的區段） |
| `NOTE.md` | 實作遇到的問題記錄（供報告統整） |
| `SOFTWARE/uCOS-II/SOURCE/` | 共用核心原始碼（已修改） |
| `EX_*/BC45/SOURCE/TEST.C` | 各組員需要填入的骨架程式 |
| `EX_*/BC45/TEST/taskset.txt` | 測試用任務參數（3 tasks，利用率 50%） |

---

## 與老師範例的差異（`ShareToVM/`）

| 項目 | 老師範例 | 本專題 |
|------|----------|--------|
| **TCB 資料儲存** | 用 `OSTCBExtPtr` 指向自訂 `TASK_EXTRA_DATA` struct，不修改 OS_TCB | 直接在 OS_TCB 加入新欄位，並擴充 `OSTaskCreateExt` 簽名接收 period / exec_time |
| **執行時間計算** | `OSTimeTick()` 每 tick 遞減 `RemainTime`，task 忙碌等待檢查 `RemainTime <= 0` | 用 `OSTimeGet()` 計算差值：`while ((OSTimeGet() - start_tick) < OSTCBExecTime)` |
| **輸入檔名** | `Input.txt` | `taskset.txt` |
| **RM 排序** | 未實作（骨架，留給學生） | 用 bubble sort 依 period 升序排列，分配優先權 1, 2, 3... |
| **EDF 排程邏輯** | 未實作（骨架，留給學生） | 在 `OS_Sched()` 的 `#ifdef SCHED_EDF` 區塊走訪 TCB 串列，動態覆寫最小 deadline task 的優先權為 1 |
| **輸出格式** | 每個 task 用固定欄位分別更新 Start Time 和 End Time（同一列原地更新） | 每次執行印兩次同一列：忙碌等待前填 `start=`（context switch 時刻），結束後填 `end=`（完成時刻） |
