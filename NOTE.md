# 實作問題記錄

> 供報告統整用，組員遇到問題請繼續補充。

---

- 修改 `OSTaskCreateExt` 簽名後，`OS_CORE.C` 內部建立 idle/stat task 的呼叫也需要同步補上新參數，否則編譯失敗。

- 主機端 SOFTWARE 資料夾內若有殘留的 `TEST.EXE`，複製到 VM 時會蓋掉剛編譯的版本，導致執行的是舊程式。每次複製後需先執行 `CLEAN.BAT` 再重新編譯。

- `TEST.EXE` 執行時以命令提示字元的當前目錄尋找 `taskset.txt`，需確認檔案放在 `TEST\` 資料夾內。

---

*（組員請在此繼續補充）*

組員C
1.一開始遇到的問題是，應該要等其他人做完才開始下一個task，變成結束時間一樣，一起結束了，雖然說總和的時間是一樣的
 解決方式:
        PeriodicTask()：拿到 semaphore 之後才開始算 execution time，所以不會再一起 end=8s
2.在排隊等待的時候，都沒有插隊，以為做好了，但在task3做完時，在等待的task2插隊優先權較高的task1
 解決方式:
        OSSemPend()：
        semaphore 空的時候，記錄 OSEventOwner = OSTCBCur
        semaphore 被持有時，Task1/Task2 都走一樣的 wait list 流程
        不再改 Task1/Task2 的 priority，不再破壞 wait list
        OSSemPost()：
        如果有人在等 semaphore，就用原本 OS_EventTaskRdy() 喚醒最高優先權等待者
        把 OSEventOwner 交給被喚醒的 task
        所以 Task3 做完後，一定先喚醒 prio 1 的 Task1，再輪 Task2
3.task2的#3消失了
        Task2 #2 看起來是：
        t=9 -> 10
        但實際 tick 可能是：
        9.x -> 10.x
        畫面會顯示 end=10，可是程式看到 end_tick > release_tick(10)，就把 release=10 判成錯過，直接跳到 15，所以 Task2 #3 出不來。
        改成：只有 release 已經落在更早的顯示秒數，才跳過。 如果 release 和 end 在同一秒，例如都是 10 秒，就 delay=0 立刻跑。
4.task1的#2missing，把#2去除換成#3
        改成：先等到 task 真的拿到 semaphore、取得 start_tick，再把 release_tick 補到對應的 release slot，最後才算 run_count。

