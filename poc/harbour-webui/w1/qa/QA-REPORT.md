# PoC-002 W1 QA report

QA engineer, WangTi team. Branch `cursor/poc-harbour-webui-a86d`, local only (no push, no PR).
Spec: `docs/team/POC-SPEC-002.md` revision 3, sections 5, 7, 7.1. Back-end report: `../W1-REPORT.md`.
Source code and spec were not edited. All commands below were re-run by QA on a clean state.
Raw outputs: `results/`.

## Verdict: W1 PASS (one minor open finding F1, wording issues listed in section 7)

| # | Item | Result |
|---|---|---|
| 1 | Re-run `run-w1.sh` and `run-valgrind.sh` | pass, numbers equal the report |
| 2 | A4 of PoC-001 | pass; by-eye only is not enough for A10, automatic check added and run |
| 3 | Rule 9 range checks, 5+ negative tests | pass (315 negative checks, 7 cases, O2 and ASan; 2 mutants detected) |
| 4 | Rules 3, 5, 12 | rule 3 pass, rule 5 pass, rule 12 not verified (not in W1 scope) |
| 5 | `show` has no size and position in W1; rule 9 ambiguity | confirmed; ambiguity described in section 7 |

## 1. Re-run (results/run-w1.out, results/run-valgrind.out)

`./run-w1.sh` exit 0, "== ALL PASSED". C89 14 checks, W0 19+14 checks, rule 7 symbol check, A13 six steps ok.

| Variant | Executor self-test | Mode 1 (linked) | Mode 2 (hrb) | A14 TAP bodies |
|---|---|---|---|---|
| O0 | 23 ok | 274 ok, 0 not ok | 274 ok, 0 not ok | identical |
| O2 | 23 ok | 274 ok, 0 not ok | 274 ok, 0 not ok | identical |
| asan | 23 ok | 274 ok, 0 not ok | 274 ok, 0 not ok | identical |
| cnt | 23 ok | 275 ok, 0 not ok | 275 ok, 0 not ok | identical |

`./run-valgrind.sh`, both modes (linked, hrb): 274 ok, 0 not ok; `CLASS bridge-webui definite_records=0 definite_direct_bytes=0`;
`CLASS gtk-glib definite_records=2 definite_direct_bytes=2065` (QA checked the raw stack of the 1-byte record:
`g_strdup` from a dlopen constructor, not bridge/WebUI); suppressed pass: 0 errors, exit code 0. All equal to the report.

Caveat on A8 "counters are 0": the `cnt` variant shows about 3614 live malloc blocks after each clean (Harbour VM, WebUI, GTK),
and the Back-end relaxed the check to "no growth" (report 6.8). The bridge-owned counter (`live_bytes`, `allocs == frees`) is 0 and is tested.
So "counter is 0" holds for bridge-owned memory only; see wording issue 8.

## 2. A4 (PoC-001 screenshot)

`poc001-check.sh` re-run: A1, A2, A3, A5 ok; A4 only saves `a4-call3.png` and prints a reminder. Assessment:
- PoC-001 A4 says "three clicks, counts in order 1, 2, 3". One screenshot taken after the third click shows only "call #3",
  not the order. Nobody checks it in a script run, so a regression would pass `poc001-check.sh` with exit 0.
- A10 says A1 to A5 "still pass". By-eye is acceptable once as a one-off confirmation, but not as regression evidence for a gate
  that is re-run per wave. Verdict: not acceptable as the only evidence; an automatic check is feasible and cheap.
- Automatic check: `qa/a4-auto.sh`. Xvfb :89, click the button three times, after each click select all (ctrl+a, ctrl+c),
  read the clipboard with `xclip`, assert the text of click N contains `Harbour says: Hello, World! (call #N)`. No OCR needed.
  Result: 3 of 3 ok, exit 0 (`results/a4-auto.out`). Negative control with 2 clicks fails as designed, exit 1
  (`results/a4-auto-negative-control.out`).
- Limits: depends on the click position (382,155) and the Chrome clipboard; same as the Back-end script.

## 3. Rule 9 negative inputs (qa/qa_neg.prg, qa/qa_num.c, qa/run-qa-neg.sh)

Code review: `bridge_arg_size` rejects non-numeric, NaN, negative, fractional, infinity, above 2^53 (`bridge_n2size`), then the range.
W1 inputs: `set_timeout` 0..60; `set_config` option 0..5 with 5 (asynchronous_response) refused, status must be logical;
window id 1..65535; handler name 1..64 chars and must be a Harbour function; element and content must be strings without NUL.
`set_config` ranges are not in rule 9; the Back-end derived them from the enum.

Tests: 7 cases (timeout 40, config 56, window 114, bind 35, show 15, state 50, table 5 = 315 ok), each case in its own process.
Inputs: NaN, -NaN, +/-inf, 2^53+2, 2^63, 2^64, -2^63, DBL_MAX, denormal, -1, -0.5, 0.5, 1.0000001, +/-1e300, -1000000,
unknown enum 5 to 65536 and 2^32, wrong types (string, NIL, logical, array, hash, codeblock, date), NUL bytes, 100,000 character names,
1 MB element, ids 1, 2, 255, 256, 257, 65534, 65535 that were never created (WebUI returns 0 or false, no crash), 600 binds.
Result: O2 and ASan+UBSan (`-fno-sanitize-recover`) all pass, no crash, no sanitizer report.
Mutation check (patched copies, real source untouched): `BRIDGE_TIMEOUT_MAX 61` fails 1 test; removing the range check fails 26 tests.
So the tests can detect an accepted out-of-range value.

Finding F1 (minor, open, not a rule 9 issue): `WEBUI_EXIT()` as the first call does not mark WebUI as used, so `WEBUI_CLEAN()` returns .F.
and does nothing, and `WEBUI_NEW_WINDOW()` returns 0 for the rest of the process (`results/f1-repro.prg`, `results/f1-repro.out`).
The Back-end's rebuild test calls `new_window` before `exit`, so it does not cover this order. Section 3 of the spec says rebuilding
after clean must be tested, not assumed. `new_window` also returns 0 silently instead of raising.

## 4. Rules 3, 5, 12

| Rule | Evidence | Status |
|---|---|---|
| 3 async queue | Code: trampoline only queues under a mutex; Harbour handlers run in `bridge_pump` on the main thread; `asynchronous_response` set by the bridge and not changeable. Tests in `t_w1.prg`: 20 concurrent calls each answered once, 20 sequential, queue empty after the run, wait inside a handler raises, pump does not re-enter, generic bind. | tested (functional). No ThreadSanitizer run; thread safety of the queue is code review only. |
| 5 clean outside callback | Code: `WEBUI_CLEAN` inside a handler only answers, exits and sets `clean_pending`; second call after `wait` runs `webui_clean`. Tests: clean cycle checks .F. in handler, pending = 1, table intact, then .T. after wait, then no-op. Also 5 clean and rebuild cycles (QA state case). | tested |
| 12 script result flag + buffer | `webui_script` is W3. No W1 function touches it; only the predicate `bridge_range_script_timeout` exists. | not verified (not applicable to W1) |

Note on rule 5 wording: handlers run on the main thread, not on WebUI's callback thread. "Callback" in the bridge means "inside a
bound handler" (`bridge_in_dispatch`). This satisfies the spec intent but the spec text does not say so.

## 5. `show` in W1

Spec section 7 lists `show` among the 12 W1 functions; no size or position function is in W1 (`size`, `position` are W4). The
bridge `WEBUI_SHOW( nWin, cContent )` has exactly two inputs. Confirmed. The rule 9 size and position limits therefore have only
predicate unit tests in W1 (`bridge_range_window_size/pos`), no Harbour-level input.

## 6. Commands run by QA

```
cd poc/harbour-webui/w1
./run-w1.sh ; ./run-valgrind.sh ; ./poc001-check.sh
qa/run-qa-neg.sh O2 ; qa/run-qa-neg.sh asan ; qa/run-qa-neg.sh O2 mutant1 ; qa/run-qa-neg.sh O2 mutant2
qa/a4-auto.sh 3 ; qa/a4-auto.sh 2
```

## 7. Spec wording issues for PM

1. Rule 9 "呼叫端必須驗證範圍": "caller" is ambiguous (Harbour application code or the bridge wrapper). The Back-end made the bridge reject; the spec
   does not say reject by error (EG_ARG) or return false (rule 7 returns false).
2. Rule 9 is not tied to a wave. It lists limits for functions of W2 to W4 (argument index, script, size, position), so W1 can
   only test `set_timeout`. Say per wave which limits are tested at Harbour level.
3. Rule 9 gaps: no range for `set_config` option and status, window id and bind id (spec silent; bridge uses 1..65535, WebUI allows 1..65534);
   "至多 86400" has no lower bound (the bridge refuses 0, rule 4 says 0 may wait forever); "乘" should read width by height;
   whether the bounds are inclusive, the unit (seconds), and non-integer input (1.5) are not stated.
4. Rule 3 says "建議" (recommended) but section 2.1 is declared binding; the Back-end and tests treat it as mandatory. Pick one word.
5. Rule 5 "非回呼執行緒" does not match the design where handlers run on the main thread; define "callback".
6. Rule 12 and `webui_script` belong to W3; the W1 request to check them has no W1 object. State the wave in the rule.
7. Section 5 A10 does not say whether PoC-001 A4 may be checked by eye; section 4 uses "A4 腳本" for a W0 ABI group, which clashes with acceptance A4.
8. A8 "計數器為 0": not defined which counter (bridge-owned bytes, or all malloc blocks). All-block counters stay at about 3614.
9. W1 function list is 12, but the bridge also exports test helpers `WEBUI_PUMP` and `WEBUI_BRIDGE_STAT`, which are not in the 104.
   Say whether helpers outside the 104 are allowed.
10. Section 3 / W1 "clean 後重建視窗": does not define clean after a lone `exit` (finding F1).
