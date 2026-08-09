# Serve Watch — Project Reference

## What this is
A C++ portfolio project for a first-year CS student targeting trading-firm SWE internships. Ingests tennis point-by-point data and computes **live derived signals** (not just scoreboard mirroring): momentum score, break-point conversion, serve dominance, anomaly flags. Built as a **replay engine** (historical CSV replayed at real-time speed) rather than depending on a live feed — mirrors how trading infra backtests against recorded data.

**Why this project (context for framing, not to build):** demonstrates event-driven systems / concurrency / infra skills relevant to trading roles, without being a literal "trading bot" (weak signal). Domain is tennis because points are discrete/hierarchical and easy to model cleanly.

## Data
CSV with per-point rows. Key columns for v1 (ignore the rest for now):
`match_id, PointNumber, PointWinner, PointServer, P1BreakPoint, P2BreakPoint, P1BreakPointWon, P2BreakPointWon, SetNo, P1GamesWon, P2GamesWon, P1Score, P2Score`

Note: the CSV already contains `P1Momentum`/`P2Momentum` columns (someone else's precomputed metric). **Do not use these as input.** Compute momentum independently from primitive columns; use the provided columns only afterward as a sanity-check comparison.

## Design principle
Build the core logic **fully in-memory first** (single-threaded, single process, stdout output). Only add sockets / threads / Redis / Postgres once the core logic works — they are architectural choices that simulate distributed-systems patterns for learning/signaling purposes, not requirements of the core logic. Each should be added with a real motivating reason at that point (see stages).

## Stages

1. **CSV parsing** — Define a `Point` struct matching the key columns above. Parse one match's rows into a `std::vector<Point>`. Print each point. Sanity-check against a known real match score.
2. **Replay/timing layer** — Loop over the `vector<Point>` with `std::this_thread::sleep_for` between points to simulate live arrival. Still single-threaded, still stdout, no networking yet.
3. **Momentum + derived metrics** — Rolling window (`std::deque<Point>` of last N points) computing: momentum score (weighted, break points > routine points), break-point conversion %, serve dominance %, simple anomaly flags (e.g. "lost 4 of last 5 on second serve"). Validate against the CSV's own `P1Momentum` column. This is the core "interesting" logic of the project.
4. **Sockets** — Split into two processes: replay engine (emits points over a socket) + client (consumes, runs stage 3 logic). Motivation: simulates a real live feed being a separate remote system. Client logic should barely change from stage 3.
5. **Threads** — Add concurrency where genuinely useful (e.g. receive vs. compute on separate threads). Motivation: don't block on I/O while computing.
6. **Redis (message queue)** — Producer publishes point events to a stream; separate consumer(s) process them. Motivation: decouples ingestion rate from processing rate; enables multiple independent consumers (e.g. live-print + persistence running independently).
7. **Postgres** — Persist points/metrics for cross-match, post-hoc queries (e.g. "does set-1 momentum predict match outcome", season-average comparisons). Add as one `save_to_db()` call at the point where output is already being produced — should not require restructuring core logic. Motivation only kicks in once a question needs data to outlive a single replay run.
8. **Stretch** — Minimal HTTP API (`cpp-httplib`) to query match history; swap replay engine for a real live API with no client-side changes.

## Libraries (plumbing only — core logic is custom)
`libcurl` (HTTP), `nlohmann/json` (JSON), `hiredis` (Redis), `libpqxx` (Postgres), later `cpp-httplib`.

## Target CLI (rough UX goal)
```
./serve_watch --player alcaraz --match replay --file ausopen_2024_final.csv
[14:22:07] POINT: Alcaraz wins (ace)
[14:22:07]   Momentum (last 10 pts): Alcaraz 0.68 | Serve points won: 8/9 (89%)
[14:31:44] BREAK POINT — Alcaraz serving, 4-4, 30-40
[14:31:52]   ALERT: Alcaraz now 1/4 on break points saved today (season avg: 68%)

./serve_watch --player alcaraz --match-summary <match_id>
```

## Ground rules for any LLM helping on this
- Don't skip ahead to stages 4–7 until stage 3 works end-to-end on real data.
- Don't use the CSV's `P1Momentum`/`P2Momentum` as an input to any metric calculation.
- Keep core metric logic (stage 3) framework/library-free — it's the part meant to demonstrate skill.
- Prefer minimal, working code at each stage over premature architecture.

## Project Structure
Detailed file layout and architectural roadmap are logged in [plan_dump.md](file:///c:/Users/angel/Repositories/tennis-point-by-point/Planning/plan_dump.md).

