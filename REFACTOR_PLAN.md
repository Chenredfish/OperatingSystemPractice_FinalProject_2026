# OS Final Project — Context Switch Output + Display Refactor Plan

**Target: one coherent change that fixes all three builds simultaneously.**

---

## 1. The Core Problem

All three schedulers (RM, EDF, PCP) share two bugs that stem from the same root cause:

### Bug A — Wall-clock busy-wait doesn't freeze during preemption
```c
while ((OSTimeGet() - start_tick) < OSTCBCur->OSTCBExecTime) { }
```
`OSTimeGet()` returns the global tick counter `OSTime`, which increments regardless of which task holds the CPU. When Task2 (exec=4s) is preempted for 1 second by Task1, the wall-clock elapsed time reaches 4s after only 3s of actual CPU work. Task2 exits the loop having done less work than intended.

**Fix**: replace with `OSTCBRunCntr` busy-wait. `OSTCBRunCntr` is charged only when the task is `OSTCBCur`, so it freezes during preemption. This field already exists in the kernel (added in commit `ebbc3b3`).

### Bug B — No context switch output
The display model is "task-centric": each task has a fixed row and overwrites it with start/end. The teacher's requirement is a time-ordered trace showing when each CPU segment ran and when preemptions occurred.

**Fix**: hook `OSTaskSwHook` in the shared CPU port file. It fires at every context switch. Log `{time, from_id, to_id}` to a ring buffer. A renderer in `TaskStart`'s idle loop drains the buffer and prints the timeline.

---

## 2. Architecture

```
OSTimeTick() [200Hz ISR]
  └─ OSTCBCur->OSTCBRunCntr++    (already in OS_CORE.C)

OSCtxSw / OSIntCtxSw [assembly, calls OSTaskSwHook before restoring context]
  └─ OSTaskSwHook()               ← YOU FILL THIS IN OS_CPU_C.C
       └─ CtxLog[CtxLogCount++] = {OSTime, OSTCBCur->OSTCBId, OSTCBHighRdy->OSTCBId}

TaskStart idle loop [every 1s, runs at prio=0]
  └─ renderer: drains CtxLog, prints one line per completed CPU segment
```

**Key invariant**: the hook runs with interrupts disabled (it is called from assembly context-switch stubs). You MUST NOT call `OSTimeGet()`, `PC_DispStr()`, or any function that re-enters the OS or uses I/O. Use only `OSTime` directly and write to a plain C array.

---

## 2.5 Display Layout (Option A — Compact Task Info)

DOS text mode is 80×25 (rows 0–24). The original layout wasted one row per task on static config info, leaving only 12 rows for the timeline. Compact format reclaims those rows.

**New layout:**

```
Row  0 : title bar
Row  2 : scheduler type
Row  4 : "Task Config:" label  (printed by TaskStartDispInit)
Row  5 : compact task info — all tasks on one line
Row  6 : overflow line if TaskCount > 4  (otherwise blank)
Row  7 : "Context Switch Trace:" header
Rows 8–23 : timeline  (16 rows)
Row 24 : ESC prompt
```

This gives **16 rows** for the timeline instead of 12.

### Compact task info format

In `TaskStartCreateTasks`, replace the per-task `sprintf`/`PC_DispStr` loop with:

```c
{
    char info[160];
    char chunk[24];
    int  col = 0;
    int  info_row = 5;
    info[0] = '\0';
    for (i = 0; i < TaskCount; i++) {
        sprintf(chunk, "T%d:e=%lds,p=%lds  ",
                i,
                (long)(TaskExecTime[i] / OS_TICKS_PER_SEC),
                (long)(TaskPeriod[i]   / OS_TICKS_PER_SEC));
        if (col + (int)strlen(chunk) > 78) {          /* wrap to row 6 */
            PC_DispStr(0, info_row, info, DISP_FGND_WHITE + DISP_BGND_BLACK);
            info[0] = '\0';
            col = 0;
            info_row++;
        }
        strcat(info, chunk);
        col += (int)strlen(chunk);
    }
    if (info[0])
        PC_DispStr(0, info_row, info, DISP_FGND_WHITE + DISP_BGND_BLACK);
}
```

For EDF, append `dead=%lds` per chunk if you want. For PCP, append `sem=%c`.

### Update renderer start row

Everywhere the renderer initialises `disp_row`, change:
- `static int disp_row = 11;` → `static int disp_row = 8;`
- `if (++disp_row > 22) disp_row = 11;` → `if (++disp_row > 23) disp_row = 8;`

### Update `TaskStartDispInit` in all three builds

Add a row-4 label and move the "Trace" header to row 7:
```c
PC_DispStr(0,  4, "Task Config:",
           DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
PC_DispStr(0,  7, "--- Context Switch Trace: each line = one CPU segment (seconds) ---",
           DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
```
Remove (or do not add) any old row-9 header line.

---

## 3. File: `SOFTWARE/uCOS-II/Ix86L/BC45/OS_CPU_C.C`

This file is **shared by all three builds** via the makefile (`PORT=\SOFTWARE\uCOS-II\Ix86L\BC45`). Changing it here updates RM, EDF, and PCP on the next build.

### 3.1 After `#include "includes.h"`, add the ring buffer definition

```c
/* ---- Context-switch ring buffer (written by OSTaskSwHook, read by TaskStart) ---- */
#define CTX_LOG_SIZE  2048

typedef struct {
    INT32U  time;       /* raw OSTime tick at the moment of the switch              */
    INT8U   from_id;    /* OSTCBId of the outgoing task  (0 = TaskStart / idle)     */
    INT8U   to_id;      /* OSTCBId of the incoming task                             */
} CTX_LOG_ENTRY;

CTX_LOG_ENTRY  CtxLog[CTX_LOG_SIZE];
INT16U         CtxLogCount = 0;   /* total entries written; never decremented        */
```

`CTX_LOG_SIZE = 2048` costs 2048 × 6 = ~12 KB — trivial for DOS large model.

### 3.2 Fill `OSTaskSwHook` (currently empty `{}` at line 289)

```c
#if OS_CPU_HOOKS_EN > 0
void  OSTaskSwHook (void)
{
    if (CtxLogCount < CTX_LOG_SIZE) {
        CtxLog[CtxLogCount].time    = OSTime;                    /* NOT OSTimeGet() */
        CtxLog[CtxLogCount].from_id = (INT8U)OSTCBCur->OSTCBId;
        CtxLog[CtxLogCount].to_id   = (INT8U)OSTCBHighRdy->OSTCBId;
        CtxLogCount++;
    }
}
#endif
```

**Why `OSTime` not `OSTimeGet()`**: `OSTimeGet()` enters a critical section (`OS_ENTER_CRITICAL`). The hook is already called from within the context-switch assembly code which has interrupts disabled. Calling `OS_ENTER_CRITICAL` again would be re-entrant on method 1/2, or save a second CPU SR on method 3. Just read `OSTime` directly — it is a single `INT32U` global and is safe to read here.

---

## 4. File: `EX_RM/BC45/SOURCE/TEST.C`

### 4.1 After `#include "includes.h"`, add extern declarations

```c
/* ---- Context-switch ring buffer (defined in OS_CPU_C.C) ---- */
#define CTX_LOG_SIZE  2048
typedef struct { INT32U time; INT8U from_id; INT8U to_id; } CTX_LOG_ENTRY;
extern CTX_LOG_ENTRY  CtxLog[CTX_LOG_SIZE];
extern INT16U         CtxLogCount;
```

Note: The struct is redefined identically here. This is legal in C — same layout, no ODR violation in C89/C90.

### 4.2 `OSTaskCreateExt` call — verify `id` parameter

In `TaskStartCreateTasks`, the current call looks like:
```c
prio = i + 1;
OSTaskCreateExt(PeriodicTask,
                (void *)(INT32U)(i + 1),
                &TaskStk[i][TASK_STK_SIZE - 1],
                prio,           /* 4th arg: priority */
                prio,           /* 5th arg: id       */   ← this is OSTCBId
                ...
```
For RM, `prio = i+1`, so `OSTCBId = i+1 = 1,2,3`. This maps directly to display_id via `display_id = OSTCBId - 1`. **RM requires no change here.**

### 4.3 Rewrite `PeriodicTask`

Replace the entire function body. Key changes:
- `run_base` variable to snapshot `OSTCBRunCntr` before the busy-wait
- Busy-wait now uses `OSTCBRunCntr - run_base` instead of `OSTimeGet() - start_tick`
- **Remove all `PC_DispStr` calls** — the timeline renderer in TaskStart handles display
- Keep `OSTCBDeadline` update, `next_arrival` absolute scheduling, and INT16U split for `OSTimeDly`

```c
void PeriodicTask(void *pdata)
{
    INT32U  next_arrival;
    INT32U  run_base;
    INT32U  delay_ticks;
    INT8U   task_id = (INT8U)(INT32U)pdata;

    next_arrival            = MyStartTime + OSTCBCur->OSTCBPeriod;
    OSTCBCur->OSTCBDeadline = next_arrival;

    for (;;) {
        /* CPU-tick busy-wait: OSTCBRunCntr freezes during preemption */
        run_base = OSTCBCur->OSTCBRunCntr;
        while ((OSTCBCur->OSTCBRunCntr - run_base) < OSTCBCur->OSTCBExecTime) { }

        /* Sleep until next absolute release */
        if (next_arrival > OSTimeGet()) {
            delay_ticks = next_arrival - OSTimeGet();
            while (delay_ticks > 60000) {
                OSTimeDly(60000);
                delay_ticks -= 60000;
            }
            OSTimeDly((INT16U)delay_ticks);
        } else {
            OSTimeDly(1);
        }

        next_arrival           += OSTCBCur->OSTCBPeriod;
        OSTCBCur->OSTCBDeadline = next_arrival;
    }
}
```

The `task_id` local variable can be kept for potential debug use; if truly unused the compiler will warn — just remove it.

### 4.4 Add renderer to `TaskStart` idle loop

Replace the existing `for(;;)` in TaskStart with:

```c
    /* State for timeline renderer */
    static INT32U seg_start_tick[MAX_TASKS + 2];   /* indexed by OSTCBId (1-based) */
    static int    disp_row = 8;    /* see Section 2.5: timeline starts at row 8 */
    static INT16U render_idx = 0;

    for (;;) {
        INT16U log_snap = CtxLogCount;   /* read once to avoid partial INT16U read */

        while (render_idx < log_snap) {
            CTX_LOG_ENTRY e   = CtxLog[render_idx];
            INT32U        t_s = (e.time >= MyStartTime)
                                ? (e.time - MyStartTime) / OS_TICKS_PER_SEC : 0;
            char          line[80];

            /* Close the outgoing task's segment */
            if (e.from_id >= 1 && e.from_id <= (INT8U)TaskCount
                && seg_start_tick[e.from_id] != 0) {
                INT32U ss = (seg_start_tick[e.from_id] >= MyStartTime)
                            ? (seg_start_tick[e.from_id] - MyStartTime) / OS_TICKS_PER_SEC : 0;
                sprintf(line, "Task%d:  start=%3lus  end=%3lus",
                        (int)e.from_id - 1,
                        (unsigned long)ss,
                        (unsigned long)t_s);
                PC_DispStr(0, disp_row, line, DISP_FGND_WHITE + DISP_BGND_BLACK);
                if (++disp_row > 23) disp_row = 8;
                seg_start_tick[e.from_id] = 0;
            }

            /* Open the incoming task's segment */
            if (e.to_id >= 1 && e.to_id <= (INT8U)TaskCount) {
                seg_start_tick[e.to_id] = e.time;
            }

            render_idx++;
        }

        if (PC_GetKey(&key) == TRUE) {
            if (key == 0x1B) {
                PC_DOSReturn();
            }
        }
        OSTimeDlyHMSM(0, 0, 1, 0);
    }
```

`seg_start_tick` and `disp_row` must be `static` so they persist across loop iterations. The `render_idx` tracks how many log entries have been processed — also must be `static`.

The renderer **closes a segment** when it sees the task being switched out (`from_id` matches), and **opens a segment** when the task is switched in (`to_id` matches). TaskStart (OSTCBId=0) and any system tasks (id > TaskCount) are silently skipped.

### 4.5 Update `TaskStartDispInit`

Follow the layout in Section 2.5. Add row-4 label and row-7 trace header; remove any old row-9 header.
Remove the old per-task `PC_DispStr(0, 5+i, ...)` loop in `TaskStartCreateTasks` and replace with the compact format from Section 2.5.

---

## 5. File: `EX_EDF/BC45/SOURCE/TEST.C`

### 5.1 After includes, same extern block as RM (copy verbatim)

### 5.2 `OSTaskCreateExt` — fix `id` parameter

**This is the critical EDF change.** EDF assigns `prio = BASE_PRIO + i = 10, 11, 12`. Currently the `id` arg (5th) is also `prio`, so `OSTCBId = 10,11,12`. The hook would record `from_id = 10` which the renderer ignores (> TaskCount). 

Change the call so the `id` argument is `(INT8U)(i + 1)`:

```c
prio = (INT8U)(BASE_PRIO + i);
OSTaskCreateExt(
    PeriodicTask,
    (void *)(INT32U)(i + 1),
    &TaskStk[i][TASK_STK_SIZE - 1],
    prio,
    (INT8U)(i + 1),        /* ← id = 1,2,3 (stable display ID, independent of dynamic prio) */
    &TaskStk[i][0],
    TASK_STK_SIZE,
    (void *)0,
    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR,
    TaskPeriod[i],
    TaskExecTime[i]
);
```

**Why this matters for EDF**: EDF's scheduler dynamically promotes the task with the earliest deadline to priority 1. `OSTCBPrio` changes at runtime. `OSTCBId` is set once at creation and never touched by the scheduler. Using `OSTCBId = i+1` gives a stable handle for the hook to identify which task is running.

### 5.3 Rewrite `PeriodicTask`

Same OSTCBRunCntr busy-wait pattern. Keep the absolute-deadline sleep calculation (`OSTCBDeadline - OSTimeGet()`). Remove all display calls.

```c
void PeriodicTask(void *pdata)
{
    INT32U  run_base;
    INT32U  end_tick;
    INT32U  delay_ticks;

    OSTCBCur->OSTCBDeadline = MyStartTime + OSTCBCur->OSTCBPeriod;

    for (;;) {
        run_base = OSTCBCur->OSTCBRunCntr;
        while ((OSTCBCur->OSTCBRunCntr - run_base) < OSTCBCur->OSTCBExecTime) { }

        end_tick = OSTimeGet();

        /* Sleep until current deadline (= next release for relative-deadline == period) */
        if (OSTCBCur->OSTCBDeadline > end_tick) {
            delay_ticks = OSTCBCur->OSTCBDeadline - end_tick;
            while (delay_ticks > 60000) {
                OSTimeDly(60000);
                delay_ticks -= 60000;
            }
            OSTimeDly((INT16U)delay_ticks);
        } else {
            OSTimeDly(1);   /* deadline missed: yield for one tick then continue */
        }

        OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;
    }
}
```

### 5.4 Add same renderer to `TaskStart` idle loop

Copy the renderer block from RM section 4.4. Change `MyStartTime` references — EDF also uses `MyStartTime`. No other changes needed.

### 5.5 Update `TaskStartDispInit` and `TaskStartCreateTasks`

Same as RM Section 4.5. Follow Section 2.5 layout. For EDF, the compact chunk can include `dead=%lds` if desired.

---

## 6. File: `EX_PCP/BC45/SOURCE/TEST.C`

### 6.1 After includes, same extern block as RM (copy verbatim)

### 6.2 Rename `GlobalStartTick` → `MyStartTime` and initialize correctly

PCP currently sets `GlobalStartTick = 0` (wall-clock from OS boot). Change to:
```c
INT32U  MyStartTime;   /* rename in the global variable list too */
```
And in `TaskStartCreateTasks`, change:
```c
GlobalStartTick = 0;
```
to:
```c
MyStartTime = OSTimeGet();
```
This aligns PCP's time baseline with RM/EDF (seconds since tasks were created, not since OS boot).

Update all references in `PeriodicTask` from `GlobalStartTick` → `MyStartTime`, and change display calculations from `start_tick / OS_TICKS_PER_SEC` to `(start_tick - MyStartTime) / OS_TICKS_PER_SEC`.

### 6.3 `OSTaskCreateExt` — fix `id` parameter

PCP creates tasks with `prio = i + 2` (prio=1 is reserved as PCP ceiling). Currently `id = prio = i+2`. 

Change to:
```c
prio = (INT8U)(i + 2);
OSTaskCreateExt(
    PeriodicTask,
    (void *)(INT32U)(i + 1),
    ...
    prio,
    (INT8U)(i + 1),   /* ← id = 1,2,3 (stable; PCP may temporarily set prio=1 but OSTCBId stays) */
    ...
```

**Why this matters for PCP**: when PCP elevates a task to the ceiling priority (prio=1), `OSTCBPrio` becomes 1 but `OSTCBId` remains `i+1`. The hook correctly identifies the task.

### 6.4 Rewrite `PeriodicTask`

Keep the semaphore acquire/release structure. Change busy-wait to OSTCBRunCntr. Remove display.

**Important**: `run_base` must be captured **after** `OSSemPend` returns. The time waiting for the semaphore should not count toward execution time.

```c
void PeriodicTask(void *pdata)
{
    INT32U  release_tick;
    INT32U  run_base;
    INT32U  end_tick;
    INT32U  delay_ticks;
    INT32U  period_ticks;
    INT32U  exec_ticks;
    INT8U   task_id;
    INT8U   err;

    task_id      = (INT8U)(INT32U)pdata;
    period_ticks = OSTCBCur->OSTCBPeriod;
    exec_ticks   = OSTCBCur->OSTCBExecTime;
    release_tick = MyStartTime;

    for (;;) {
        if (TaskUsesSem[task_id - 1])
            OSSemPend(SharedSem, 0, &err);

        /* run_base AFTER semaphore: we measure CPU time spent in the critical section */
        run_base = OSTCBCur->OSTCBRunCntr;
        while ((OSTCBCur->OSTCBRunCntr - run_base) < exec_ticks) { }

        end_tick = OSTimeGet();

        if (TaskUsesSem[task_id - 1])
            OSSemPost(SharedSem);

        /* Advance release_tick to next period boundary after end_tick */
        release_tick += period_ticks;
        while (release_tick <= end_tick)
            release_tick += period_ticks;

        delay_ticks = release_tick - end_tick;
        while (delay_ticks > 60000) {
            OSTimeDly(60000);
            delay_ticks -= 60000;
        }
        if ((INT32S)delay_ticks > 0)
            OSTimeDly((INT16U)delay_ticks);
    }
}
```

### 6.5 Add same renderer to `TaskStart` idle loop

Same renderer block from section 4.4. PCP now uses `MyStartTime` so the reference is consistent.

### 6.6 Update `TaskStartDispInit` and `TaskStartCreateTasks`

Same as RM Section 4.5. Follow Section 2.5 layout. For PCP, the compact chunk can include `sem=%c` to show which tasks use the semaphore.

---

## 7. Critical Gotchas

### G1 — Never call `OSTimeGet()` inside `OSTaskSwHook`
`OSTimeGet()` calls `OS_ENTER_CRITICAL()`. The hook is already executing with interrupts disabled (called from `OSCtxSw` or `OSIntCtxSw` assembly stubs). On critical section method 1/2, calling `OS_ENTER_CRITICAL` inside the hook would clobber the saved state. On method 3, it pushes a second CPU SR. Either way, undefined behavior. Use `OSTime` directly (the raw global INT32U counter).

### G2 — `OSTCBCur` vs `OSTCBHighRdy` in the hook
At the time `OSTaskSwHook` is called:
- `OSTCBCur` = the task being switched **out** (losing CPU)
- `OSTCBHighRdy` = the task being switched **in** (gaining CPU)

This is documented in the comment block above the hook in `OS_CPU_C.C`.

### G3 — `CTX_LOG_SIZE` must be identical in `OS_CPU_C.C` and each `TEST.C`
The `extern` declaration in TEST.C references an array of size `CTX_LOG_SIZE`. If the define differs between files, the renderer may read out-of-bounds. Keep the `#define CTX_LOG_SIZE 2048` value consistent.

### G4 — `static` keyword on renderer state variables
`seg_start_tick`, `disp_row`, and `render_idx` inside the `TaskStart` for-loop **must** be declared `static`. Without it, they are re-initialized to zero on every loop iteration (every second), wiping all accumulated state.

### G5 — `OSTCBId` for `TaskStart` is 0, not 1
`TaskStart` is created with `OSTaskCreate` (not `OSTaskCreateExt`). `OSTaskCreate` internally calls `OS_TCBInit` with `id = prio = 0`. In the renderer, `e.from_id == 0` means TaskStart gave up the CPU — skip it. The check `e.from_id >= 1 && e.from_id <= TaskCount` handles this silently.

### G6 — Renderer races with hook on `CtxLogCount`
`CtxLogCount` is a 16-bit value written by the hook (ISR context) and read by the renderer (task context). On 8086 in real mode, a 16-bit read is not atomic (two byte reads). The fix: read `CtxLogCount` once into a local `log_snap` before the while loop. If a hook fires between the read and the loop, `log_snap` is stale by at most one entry — that entry will be picked up in the next renderer cycle (every 1 second). This is acceptable.

### G7 — EDF `delay_ticks` after OSTCBRunCntr
With OSTCBRunCntr, the task finishes its exec time later than with wall-clock (correctly). The sleep calculation `OSTCBDeadline - end_tick` might become very small or negative if the deadline was tight. The `if (OSTCBCur->OSTCBDeadline > end_tick)` guard handles the missed-deadline case by yielding one tick.

### G8 — PCP `OSSemPend` timeout parameter
`OSSemPend(SharedSem, 0, &err)` with timeout=0 means **wait forever**. This is correct. While waiting, the task is blocked (not running), so `OSTCBRunCntr` is not incremented. The `run_base` snapshot after `OSSemPend` returns therefore correctly excludes the wait time.

### G9 — Build order after editing `OS_CPU_C.C`
All three builds read from the same source file `Ix86L/BC45/OS_CPU_C.C`. But each build's makefile **COPYs** it to its local WORK directory before compiling. This means you must rebuild all three after changing OS_CPU_C.C. The makefile dependency should handle this automatically if you run `make` from each build directory.

### G10 — `disp_row` wrap clears old output
When `disp_row` wraps from 22 back to 11, new output overwrites old lines. This is intentional (scrolling display). Rows 11-22 give 12 visible context switch segments at any time. For longer demos, increase the display window or add a scrolling offset. For the project demo, 12 lines is sufficient.

---

## 8. Build Order

1. Edit `SOFTWARE/uCOS-II/Ix86L/BC45/OS_CPU_C.C` (ring buffer + hook)
2. Edit `EX_RM/BC45/SOURCE/TEST.C`
3. Edit `EX_EDF/BC45/SOURCE/TEST.C`
4. Edit `EX_PCP/BC45/SOURCE/TEST.C`
5. Rebuild RM: run the `TEST.MAK` from `EX_RM/BC45/TEST/`
6. Rebuild EDF: same from `EX_EDF/BC45/TEST/`
7. Rebuild PCP: same from `EX_PCP/BC45/TEST/`

Each build COPYs `OS_CPU_C.C` fresh from the PORT directory before compiling, so all three pick up the ring buffer changes.

---

## 9. Testset for Verifying Preemption Display

Use a taskset that guarantees preemption so the timeline shows multiple segments per task.

**RM preemption test** (`EX_RM/BC45/TEST/taskset.txt`):
```
2
1 4
4 10
```
Task0: exec=1s period=4s (prio=1, higher)  
Task1: exec=4s period=10s (prio=2, lower)  
Utilization = 1/4 + 4/10 = 0.65 ≤ ln(2) ≈ 0.693 → schedulable

Expected timeline (hook-based):
```
Task1:  start=  0s  end=  1s     ← Task1 runs t=0 to 1
Task0:  start=  1s  end=  5s     ← Task0 preempts at t=1... wait this is wrong
```

Actually with RM priority assignment: shorter period = higher priority. Task0 (period=4) has prio=1. Task1 (period=10) has prio=2.

With both released at t=0:
- t=0: Task0 runs (higher prio), ends at t=1
- t=1: Task1 runs (only ready task), gets preempted at t=4 by Task0
- t=4: Task0 runs, ends at t=5
- t=5: Task1 resumes, finishes at t=6 (has done 3s CPU already, needs 1 more)
- etc.

Expected hook output:
```
Task0:  start=  0s  end=  1s
Task1:  start=  1s  end=  4s    ← partial, preempted
Task0:  start=  4s  end=  5s
Task1:  start=  5s  end=  6s    ← resumes and completes
```

**EDF / PCP**: use a similar 2-task preempting set for initial verification.

---

## 10. Final Verification Checklist

After implementing all changes, verify each item:

- [ ] **RM compiles** without errors or warnings about undefined `CTX_LOG_SIZE`, `CTX_LOG_ENTRY`, `CtxLog`, `CtxLogCount`
- [ ] **EDF compiles** same
- [ ] **PCP compiles** same
- [ ] **RM runs**: with 2-task preemption testset, rows 11-22 show time-ordered segments; Task1 appears twice (split by Task0 preemption)
- [ ] **RM correctness**: with OSTCBRunCntr busy-wait, Task1 (exec=4s) completes at wall-clock t=6, not t=5 (i.e., truly does 4 CPU seconds despite 1s preemption)
- [ ] **EDF runs**: tasks display with id=1,2,3 (not 10,11,12) in the timeline
- [ ] **EDF timeline**: earliest-deadline task always starts first; dynamic priority changes are invisible in the output (only CPU segments matter)
- [ ] **PCP runs**: semaphore-holding task shows correct segment boundaries; ceiling protocol prevents priority inversion (low-prio task holding sem is promoted before high-prio task blocks)
- [ ] **No crash** if CTX_LOG_SIZE is exhausted (CtxLogCount >= 2048): hook silently stops logging, display freezes at last entry — this is acceptable
- [ ] **ESC key** exits cleanly in all three builds
- [ ] **TaskStart row 9** shows updated header text
- [ ] **Rows 5-9** still show static task configuration (exec/period/prio) — these are printed by `TaskStartCreateTasks` and should remain unchanged

---

## Appendix: Symbol Reference

| Symbol | Location | Type | Meaning |
|--------|----------|------|---------|
| `OSTime` | `OS_CORE.C` global | `INT32U` | Raw tick counter, safe to read in hook |
| `OSTCBCur` | kernel global | `OS_TCB *` | TCB of task currently losing CPU (in hook) |
| `OSTCBHighRdy` | kernel global | `OS_TCB *` | TCB of task about to gain CPU (in hook) |
| `OSTCBRunCntr` | `OS_TCB` field | `INT32U` | CPU ticks charged to this task; freezes during preemption |
| `OSTCBId` | `OS_TCB` field | `INT16U` | Set at creation via 5th arg of `OSTaskCreateExt`; never changed by scheduler |
| `OSTCBDeadline` | `OS_TCB` field | `INT32U` | Absolute deadline tick (used by EDF scheduler and all three task bodies) |
| `OSTCBExecTime` | `OS_TCB` field | `INT32U` | Execution time in ticks (set at creation, read by task body) |
| `OSTCBPeriod` | `OS_TCB` field | `INT32U` | Period in ticks (set at creation) |
| `CTX_LOG_SIZE` | defined in `OS_CPU_C.C` and each `TEST.C` | `#define` | Must be identical in all files |
| `CtxLog[]` | defined in `OS_CPU_C.C` | `CTX_LOG_ENTRY[]` | Ring buffer; hook appends, renderer drains |
| `CtxLogCount` | defined in `OS_CPU_C.C` | `INT16U` | Write index (never decrements); read once per renderer cycle |
