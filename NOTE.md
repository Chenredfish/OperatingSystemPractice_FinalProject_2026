# 實作問題記錄

> 供報告統整用，組員遇到問題請繼續補充。

---

- 修改 `OSTaskCreateExt` 簽名後，`OS_CORE.C` 內部建立 idle/stat task 的呼叫也需要同步補上新參數，否則編譯失敗。

- 主機端 SOFTWARE 資料夾內若有殘留的 `TEST.EXE`，複製到 VM 時會蓋掉剛編譯的版本，導致執行的是舊程式。每次複製後需先執行 `CLEAN.BAT` 再重新編譯。

- `TEST.EXE` 執行時以命令提示字元的當前目錄尋找 `taskset.txt`，需確認檔案放在 `TEST\` 資料夾內。

---

*（組員請在此繼續補充）*
