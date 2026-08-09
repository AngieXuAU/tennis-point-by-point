# Soccer Watch — Project Reference

## What this is
A C++ portfolio project built to demonstrate low-latency systems / concurrency skills relevant to
market-data / trading infra roles, using live soccer match events instead of financial data (avoids
the "quant-bait" red flag of a literal trading project, while still speaking directly to the skills
those teams screen for). Companion project to **Serve Watch** (tennis) — this one deliberately keeps
the *domain logic* trivial so the *systems engineering* can be the deliverable.

**Core idea:** replay a recorded match as a paced live feed over a socket, using a custom binary wire
protocol, into a consumer that reconstructs live match state through two interchangeable queue
implementations (mutex-based vs. lock-free SPSC ring buffer) — then benchmark the two head-to-head on
throughput and, more importantly, tail latency, under realistic bursty load.

**Why soccer, specifically:** match events are naturally bursty — long quiet stretches, then several
events within seconds of each other (shot → save → corner → shot). That burst pattern is exactly the
condition under which mutex-vs-lock-free differences actually show up. It also keeps the "why this
domain" answer honest: not a finance reskin, a genuinely different data source that happens to share
the shape (discrete timestamped events → running state) that makes the comparison meaningful.

## Relationship to Serve Watch
Both projects will eventually share one small generic component: a **paced event-replay/scheduler**
(read timestamped records, emit them at real-time — or scaled — speed). That piece knows nothing about
tennis or soccer; it just knows "events with timestamps." Each project's *consumer* logic (momentum
scoring vs. live match state) stays fully separate and domain-specific — no logic is shared, only the
pacing/scheduling infra. Build this shared piece once Serve Watch stage 2 and Soccer Watch stage 2 both
exist, by extracting the common part rather than designing it up front.

## Data
Recorded match events, one per line, JSON or a simple delimited format for v1 (human-readable while
building — you'll design your own compact binary format for the wire protocol later, that's a separate
step from how you store the source file). Minimum event fields needed:

`match_id, event_type, timestamp, team, player (optional), detail (e.g. xG value, on_target bool)`

Event types for v1: `KickOff, Shot, SaveMade, Goal, Card, Substitution, FullTime`. Keep it to these —
resist adding more until the pipeline works end-to-end.

## Design principle
Same rule as Serve Watch: get the naive version fully working before optimizing. Concretely — build
the **mutex + queue** version first, prove it's correct (state matches a known real match), *then*
build the lock-free version as a drop-in replacement behind the same interface. The optimization is
never allowed to be the reason correctness is hard to verify.

## Target CLI (rough UX goal)

Two processes, one terminal each:

```
# Terminal 1 — feed
./soccer_feed --file arsenal_vs_city_2024.jsonl --speed 1x --port 9001
[19:00:00] Feed started, listening on port 9001, waiting for consumer...
[19:00:03] Consumer connected.
[19:00:14] SENT: KickOff  (match_id=4821, ts=19:00:14.002)
[19:03:41] SENT: Shot     (team=ARS, player=Saka, xG=0.08, on_target=true)
[19:11:02] SENT: Goal     (team=ARS, player=Saka, assist=Odegaard)
...
[20:52:10] SENT: FullTime
[20:52:10] Feed complete. 341 events sent. Shutting down.
```

```
# Terminal 2 — consumer
./soccer_watch --connect localhost:9001 --mode mutex   # or --mode lockfree

[19:00:14] KICKOFF — Arsenal vs Man City
[19:03:41] SHOT — Saka (ARS), xG 0.08, on target
[19:03:41]   Live state: Shots 1-0 | xG 0.08-0.00 | Possession 58%-42%
[19:11:02] GOAL — Saka (ARS), assisted by Odegaard
[19:11:02]   ALERT: Arsenal have scored from 3 of last 4 shots in box
...
[20:52:10] FULL TIME — Arsenal 2-1 Man City

--- Run stats ---
Events processed: 341
Throughput: 84,200 events/sec (burst) | 340 events/sec (sustained)
Latency (queue→applied): p50 12us | p99 340us | p999 2.1ms
Queue mode: mutex+std::queue
```

Running the identical replay file again with `--mode lockfree` should produce matching match state and
a second stats block for direct comparison. **The burst throughput and p99/p999 latency numbers are
the actual deliverable of this project** — sustained rate will look similar in both modes because a
real match only produces ~300-400 events over two hours; the difference only shows up under burst load
and in the tail.

## Stages

1. **Event parsing** — Define an `Event` struct. Parse a `.jsonl` (or CSV) file of recorded match
   events into a `std::vector<Event>`. Print each one. Sanity-check against a real match's known
   timeline. *(No sockets, no threads, single process — same spirit as Serve Watch stage 1.)*

2. **Match state reconstruction** — Apply events in order to a simple `MatchState` struct (score,
   shots, xG running totals, possession %). Print state after each event. Still single-threaded,
   single process, stdout only. Get this fully correct before anything else — it's your ground truth
   for every later stage.

3. **Naive pipeline: two threads, mutex + `std::queue`** — Split into a "feed" thread (reads/paces
   events) and a "consumer" thread (pops events, applies to `MatchState`, prints). Still one process
   — no sockets yet, just two `std::thread`s talking through a mutex-protected queue. This is your
   correctness and performance *baseline*.

4. **Baseline benchmarking** — Add timestamps at "event queued" and "event applied to state." Compute
   throughput (sustained and burst) and latency percentiles (p50/p99/p999) for the mutex version.
   Write these numbers down — you'll be comparing against them for the rest of the project.

5. **Custom binary wire protocol + sockets** — Split into two real processes. Design a small fixed-width
   binary format for events (not JSON — this is deliberate, see the C++ skill chain below). Feed process
   sends over a TCP socket; consumer process parses the binary format on receive. Motivation: simulates
   a real feed being a separate remote system, and gives you a genuine reason to hand-write
   serialization instead of using a library.

6. **SPSC lock-free ring buffer** — Inside the consumer process, replace the mutex + `std::queue`
   between "socket-receive thread" and "apply-to-state thread" with your own lock-free SPSC ring
   buffer. Re-run the exact benchmark from stage 4. This comparison — same data, same machine, only the
   queue implementation changed — is the core result of the project.

7. **Tuning + `perf`** — Cache-line-align the ring buffer's head/tail counters to eliminate false
   sharing, experiment with memory ordering choices, profile with `perf stat`/`perf record`. Measure
   each change; don't assume.

8. **Stretch — fan-out to a second consumer** — Add a second independent reader off the same event
   stream (e.g. one thread updates live `MatchState`, another independently tracks a simple rolling
   stat like "shot conversion rate last 10 minutes"). This moves from SPSC toward a harder
   multi-consumer problem — treat it as optional, only after 1–7 are solid.

## Ground rules for any LLM helping on this
- Don't skip to sockets/lock-free stages until stage 2 (match state logic) is fully correct on real data.
- Match-state / derived-stat logic (stage 2) should stay deliberately simple — this project's value is
  the pipeline, not the soccer insight. Resist the urge to add clever analytics here.
- The mutex version (stage 3-4) must be kept working and benchmarked, not deleted once the lock-free
  version exists — the comparison between them *is* the deliverable.
- Prefer minimal, working code at each stage over premature architecture, same as Serve Watch.

## C++ skill dependency chain

Read top to bottom — each skill is a prerequisite for the one(s) below it. Right column shows where it
gets used in the stages above.

```
1. Structs, vectors, file I/O, string parsing
   (basic C++ you likely already have)
        │
        ▼
2. std::thread — creating, joining, basic producer/consumer
   with a plain std::mutex + std::queue + std::condition_variable
        │                                              → used directly in Stage 3
        ▼
3. RAII / std::lock_guard, std::unique_lock
   (why you almost never call .lock()/.unlock() manually)
        │                                              → makes Stage 3 correct & exception-safe
        ▼
4. Benchmarking basics — std::chrono::steady_clock,
   collecting a vector of durations, computing percentiles
        │                                              → used directly in Stage 4
        ▼
5. Raw byte manipulation — memcpy into/out of structs,
   endianness, fixed-width integer types (uint32_t etc.)
        │                                              → used directly in Stage 5 (wire format)
        ▼
6. Sockets — POSIX sockets or a thin wrapper (Boost.Asio
   is fine to use here, it's plumbing not core logic)
        │                                              → used directly in Stage 5
        ▼
7. std::atomic basics — atomic counters shared between
   two threads, no mutex, seeing it just work
        │
        ▼
8. Lock-free SPSC ring buffer — fixed array + atomic
   head/tail indices, single producer thread, single
   consumer thread
        │                                              → used directly in Stage 6
        ▼
9. Cache-line alignment (alignas, false sharing) +
   memory_order_acquire/release semantics
        │                                              → used directly in Stage 7
        ▼
10. perf (perf stat, perf record/report) — reading cache
    miss rates, instructions-per-cycle, branch mispredicts
        │                                              → used directly in Stage 7
        ▼
11. (Stretch) MPSC / multi-consumer coordination —
    harder than SPSC, only attempt once 1–10 are solid
                                                        → used directly in Stage 8 (stretch)
```

A few notes on the chain:
- **Steps 1–4 have no lock-free content at all** — that's intentional. You should be able to fully
  build and benchmark the "boring" version (Stages 1–4) using only thread/mutex/chrono knowledge you
  can pick up from any standard C++ concurrency tutorial.
- **Steps 5–6 (binary parsing, sockets) are independent of 7–10 (atomics, lock-free)** — you can learn
  them in either order, or in parallel. They only combine at Stage 5 of the project.
- **Step 8 is the payoff step** — everything from step 2 onward exists to make step 8 possible and,
  crucially, *measurable against a working baseline* rather than trusted on faith.
- Don't attempt step 9 (memory ordering) before step 8 works with the simplest-possible atomic usage
  (`memory_order_seq_cst`, the default/safest ordering). Get it correct-but-not-optimal first, then
  relax the ordering and re-verify correctness, same as any other optimization pass.
