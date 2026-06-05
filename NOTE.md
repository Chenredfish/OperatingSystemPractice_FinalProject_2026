# 實作過程遇到的問題

> 供報告統整用，組員遇到問題請繼續補充。

---

## 問題一：OSTaskCreateExt 簽名修改後，OS 內部呼叫未同步更新

**發生位置：** `SOFTWARE/uCOS-II/SOURCE/OS_CORE.C`

**現象：** 編譯時出現 `Too few parameters in call to 'OSTaskCreateExt'`，錯誤在 `OS_InitTaskIdle`（約第 736 行）和 `OS_InitTaskStat`（約第 789 行）。

**原因：** 我們為了傳入 `period` 和 `exec_time`，修改了 `OSTaskCreateExt` 的函數簽名（在末端新增兩個 `INT32U` 參數）。但 uC/OS II 本身在初始化 idle task 和 stat task 時也呼叫了同一個函數，這兩處內部呼叫沒有一起更新。

**解法：** 在 `OS_CORE.C` 的四處 `OSTaskCreateExt` 呼叫末端加上 `0, 0`（system task 不需要週期，period 和 exec_time 設為 0）。EDF 排程邏輯中的過濾條件 `OSTCBPeriod > 0` 確保這些 system task 不會被當成排程候選。

---

## 問題二：主機端殘留舊的 TEST.EXE，複製到 VM 時蓋掉正確版本

**發生位置：** `EX_RM/BC45/TEST/TEST.EXE`（EX_EDF、EX_PCP 同樣問題）

**現象：** 明明已複製最新 SOFTWARE 資料夾到 VM，執行的卻還是舊版程式（舊作業的倒數程式）。

**原因：** 編譯產物（`TEST.EXE`、`TEST.MAP`）被 `.gitignore` 排除在 git 追蹤之外，但實際存在於主機的工作資料夾中。每次將 SOFTWARE 複製到 VM，這些舊 EXE 就一起蓋上去，讓 VM 的新版被覆蓋。

**解法：** 
- 從主機端刪除所有 `TEST.EXE` 和 `TEST.MAP`。
- 各資料夾新增 `CLEAN.BAT`，在 VM 上每次重新複製後先執行清理再編譯。
- 原則：**不要把 VM 編譯產生的 EXE 複製回主機**。

---

## 問題三：taskset.txt 路徑不符

**發生位置：** 執行 `TEST.EXE` 時

**現象：** 程式執行後顯示 `ERROR: cannot open taskset.txt`。

**原因：** `TEST.C` 用相對路徑 `fopen("taskset.txt", "r")` 開檔，程式執行時的當前目錄是命令提示字元所在位置（即 `TEST\`）。而 `taskset.txt` 原本不存在於該資料夾。

**解法：** 在三個專題資料夾的 `TEST\` 目錄各放一份 `taskset.txt`（3 個 task，總使用率 50%，符合規格上限 65%）。

---

## 補充說明：骨架程式正常執行後畫面空白

**現象：** 編譯成功、執行 TEST.EXE 後只顯示標題列，沒有任何 task 輸出。

**說明：** 這是正常現象。骨架中 `TaskStartCreateTasks()` 只讀取 taskset.txt 的資料，task 建立的程式碼留給組員填入（TODO）。沒有 task 被建立，OS 只執行 idle task 和 stat task，因此畫面空白。填入實作後才會有輸出。

---

*（組員請在此繼續補充各自遇到的問題）*
