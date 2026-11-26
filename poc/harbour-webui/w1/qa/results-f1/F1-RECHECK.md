# QA re-check of finding F1 at commit 9ac40af

Date 2026-11-26. QA worked in a detached worktree of 9ac40af (`/tmp/qa-f1`), no source or spec edits, nothing pushed.
The WebUI static library is the untracked `dist/libwebui-2-static.a` of the main checkout (symlinked into the worktree).
Bridge objects were built by `build.sh O2` at 9ac40af. All programs run one process each, under Xvfb.

## Verdict: F1 PASS (with one wording note on the task description, see 1)

## 1. Original repro, `f1-repro.prg` (exit, clean, new_window, clean, exit, clean, new_window)

Output (`f1-repro.out`):

```
exit NIL
clean .T.
new_window          1
clean .T.
exit NIL
clean .T.
new_window          1
```

Before the fix (`../results/f1-repro.out`): `clean .F.` and `new_window 0` for the whole process. Now `clean` after a lone `exit`
returns .T. and `new_window` after that clean returns a real window id (1), twice.

Note: in this exact order `clean` comes before `new_window`, so `new_window` is legal and does not raise. The raise
applies to `new_window` between `exit` and `clean`. That order is checked by `f1-repro-recover.prg` (same calls, but
`new_window` is tried before `clean` and the error is caught with BEGIN SEQUENCE):

```
exit NIL
RAISED: WEBUI_NEW_WINDOW: the application was exited; call WEBUI_CLEAN() first | WEBUI_NEW_WINDOW | subcode       3012
clean .T.
new_window (after clean) nonzero .T.
exit NIL
clean .T.
```

So: new_window raises a Harbour runtime error (subcode 3012) before clean, and after clean a window can be rebuilt. Pass.

## 2. Extra checks (not in the Back-end test)

| File | Sequence | Result |
|---|---|---|
| `f1-ext-a.prg` | exit, clean, new_window, show, wait, exit, clean (one process; Chrome killed after 8 s by `run-wait-case.sh` so wait returns) | pass: clean .T., window id nonzero, show .T., wait returned, exit, clean .T., exit code 0 |
| `f1-ext-b.prg` | second process, exit first again: exit, clean, new_window, exit, clean | pass: clean .T. both times, window nonzero, exit code 0 |
| `f1-ext-c.prg` | new_window, exit, clean, then 2 rounds of new_window, exit, clean, then one more exit and clean | pass: every new_window nonzero, every clean .T., exit code 0 |
| `f1-ctl-show.prg` | control without exit first: new_window, show, wait, exit, clean | same behaviour as ext-a, so the browser step is not changed by the exit-first order |

Harness notes (not product findings): `WEBUI_WAIT()` blocks while a browser window is connected, and `WEBUI_SET_TIMEOUT(1)` does not
end the wait for a connected page, so the harness kills Chrome. The page string must contain `<script src="webui.js">`,
otherwise `show` returns .F. after about 20 s (first attempt, discarded; the control shows it is not related to F1).

## 3. ThreadSanitizer report (archived at `../../results/tsan-linked.report`, `tsan-hrb.report`)

- Frames in the report: `free` (libtsan interceptor), `worker_thread` (civetweb), `strlen` (interceptor), `mg_strcasestr` (civetweb),
  `mg_start_thread_with_id` (civetweb), `_webui_receive` (webui.c, creates thread T67). No frame of the bridge (`bridge_*`,
  `HB_FUN_*`, queue, trampoline, pump). Both modes show the same single warning. So the report involves only WebUI/civetweb code.
  Limits: the WebUI frames print as `<null>` (no symbols or debug info), and T67's stack has only 2 frames, so a bridge callback
  below those frames cannot be excluded from the report alone; the bridge is just not visible in it.
- Build: `build.sh tsan` compiles the bridge objects and tests with `-fsanitize=thread` and links with `-fsanitize=thread`. The WebUI library is the
  prebuilt `dist/libwebui-2-static.a`. `tsan-lib-check.txt`: `civetweb.o` and `webui.o` have 0 `__tsan` references (and no debug info),
  so they were compiled without TSan instrumentation. TSan still intercepts pthread calls, but plain and atomic memory accesses
  inside the library are not instrumented for ordering, so synchronisation done by atomics or by lock-free handoff there is invisible.
- Verdict: UNPROVEN. Not shown to be a false positive, and not shown to be a real race. Reasons it could be real: T67 is a
  thread started from the connection worker that read a header buffer, and the same worker freed it. Reasons it could be a
  false positive: uninstrumented library code, missing happens-before edges. No bridge involvement was observed. The right
  confirmation is to build WebUI with TSan too; QA did not do this (out of scope, no fix).

## 4. W1 matrix, `run-w1.sh O2` only (O0, asan, cnt not run)

Three runs on the same tree:

- Run 1 (`w1-O2-run1.log`): static checks, A13 pass. O2 mode hrb 297 ok. O2 mode linked **2 not ok** (`[exit] show returns .T. (browser started)` and
  `[exit] page ran to the end and reported`, tests 195 and 196), so mode bodies differ (`w1-O2-run1-linked-FAILED.tap`, `w1-O2-run1-diff.txt`).
  This run directly followed QA's own Chrome/Xvfb sessions that were killed by timeout, and two stale Xvfb servers were still running (found afterwards).
- Run 2 (`w1-O2-run2.log`) and run 3 (`w1-O2-run3.log`, run after stopping the stale Xvfb processes): ALL PASSED, 297 ok and 0 not ok in both modes, identical TAP bodies.

Result: pass on 2 of 3 runs. The one failure is a browser start failure in the GUI part of the `[exit]` scenario. The cause
was not established (QA-side interference is likely but not proven), so it is recorded as a possible flake and not as an F1 regression.
F1-related tests (`ExitOrderTests`, headless) passed in all three runs.
