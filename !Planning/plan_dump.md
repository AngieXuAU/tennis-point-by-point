*2026.08.08*

# Project Structure Design — Serve Watch

## Overview & Architectural Goals
The objective of **Serve Watch** is to build an event-driven tennis replay engine and signal processor in modern C++. The architecture must begin minimalist (single-threaded, in-memory, zero-dependency except standard library) for Stages 1–3, while seamlessly supporting eventual multi-process socket IPC, multi-threading, Redis streaming, Postgres persistence, and HTTP APIs in Stages 4–8 without requiring refactoring of core domain logic.

---

## Directory Tree

```
tennis-point-by-point/
├── CMakeLists.txt              # Primary CMake build system configuration
├── README.md                   # Project overview & build instructions
├── .gitignore                  # Git ignore rules (build targets, temp files)
│
├── Planning/                   # Architecture reference, progress logs, & planning
│   ├── tennis_watch_reference.md
│   ├── plan_dump.md
│   ├── progress_log.md
│   └── progress_summary.md
│
├── data/                       # Historical point-by-point dataset storage
│   └── ausopen_2024_final.csv  # Sample raw CSV data for testing & replay
│
├── include/                    # Public header files (or modular src/ headers)
│   └── tennis_watch/
│       ├── core/               # Pure domain models & mathematical metric engines (No I/O dependencies)
│       │   ├── point.hpp       # Point struct (matches key CSV fields)
│       │   ├── momentum.hpp    # Rolling momentum calculation logic
│       │   └── metrics.hpp     # Derived stats (Break point conversion, serve dominance, anomalies)
│       │
│       ├── ingestion/          # Data feed & replay interfaces
│       │   ├── csv_parser.hpp  # Fast, robust CSV ingestion into std::vector<Point>
│       │   └── replay_engine.hpp # Real-time simulation replay loop (timing / sleep_for layer)
│       │
│       ├── presentation/       # CLI formatting & user output
│       │   └── cli_formatter.hpp # Rich stdout log formatting & alerts
│       │
│       ├── networking/         # IPC & Sockets (Stage 4+)
│       │   └── socket_feed.hpp # Socket producer/consumer abstractions
│       │
│       ├── concurrency/        # Concurrency primitives (Stage 5+)
│       │   └── thread_pool.hpp # Thread management & thread-safe event queue
│       │
│       └── storage/            # External integrations (Stage 6–7)
│           ├── redis_stream.hpp# Redis producer/consumer stream wrapper (hiredis)
│           └── db_repository.hpp# PostgreSQL persistence client (libpqxx)
│
├── src/                        # Implementation files (.cpp)
│   ├── main.cpp                # CLI entry point (`./tennis_watch`)
│   ├── core/
│   │   ├── momentum.cpp
│   │   └── metrics.cpp
│   ├── ingestion/
│   │   ├── csv_parser.cpp
│   │   └── replay_engine.cpp
│   ├── presentation/
│   │   └── cli_formatter.cpp
│   ├── networking/             # (Stage 4+)
│   ├── concurrency/            # (Stage 5+)
│   └── storage/                # (Stage 6-7+)
│
└── tests/                      # Unit & integration testing
    ├── unit/
    │   ├── test_csv_parser.cpp # Test edge cases & field validation
    │   └── test_metrics.cpp    # Test momentum & derived metrics calculations
    └── CMakeLists.txt          # Test build targets (e.g. Catch2 or GoogleTest)
```

---

## Rationale & Stage Evolution

### 1. Core Domain Isolation (`include/tennis_watch/core/`)
- **Stage 1–3 Focus**: Defines the primitive data structures (`Point`) and pure functions/classes (`MomentumEngine`, `MetricsEngine`).
- **Zero I/O Rule**: Core metric logic contains no networking, database, or direct printing calls. It consumes raw point structs and updates internal rolling state (`std::deque<Point>`).
- **Testability**: Allows unit testing metric calculations against expected math without needing socket setups or CSV files.

### 2. Ingestion & Replay Layer (`include/tennis_watch/ingestion/`)
- **Stage 1**: `CSVParser` parses `.csv` rows into `std::vector<Point>`.
- **Stage 2**: `ReplayEngine` wraps the vector and emits points to a listener callback or channel at timed intervals (`sleep_for`).
- **Stage 4+**: Abstract feed interfaces allow `ReplayEngine` to publish via Sockets or Redis streams instead of direct in-memory calls.

### 3. Presentation Layer (`include/tennis_watch/presentation/`)
- Handles CLI flags (`--player`, `--match`, `--file`) and formats rich stdout notifications and alerts matching target UX.

### 4. Infrastructure & Integration Layers (`networking/`, `concurrency/`, `storage/`)
- Kept strictly segregated so that early stages remain fast to compile and clean to inspect.
- Added incrementally at Stages 4, 5, 6, and 7 as motivated by concurrency, decoupling, and persistent storage needs.

---

*2026.08.09*

# Stage 1 Implementation Strategy: CSV Parser Setup

## Setup Workflow: Directory Structure First
- **Strategy**: Create the dedicated directory paths upfront (`include/tennis_watch/core`, `include/tennis_watch/ingestion`, `src/ingestion`) rather than writing code in a scratch file and moving it later.
- **Rationale**: 
  - Prevents breaking header inclusion paths (`#include "tennis_watch/core/point.hpp"`) later.
  - Keeps build tool configurations (`CMakeLists.txt`) clean and predictable from day one.
  - Matches production project workflows where modular boundaries are established early.

## Step-by-Step Execution Sequence for Stage 1

1. **Step 1: Create Minimal Folders**
   - Create directories:
     - `include/tennis_watch/core/`
     - `include/tennis_watch/ingestion/`
     - `src/ingestion/`
     - `src/`
     - `data/`

2. **Step 2: Define Domain Struct (`point.hpp`)**
   - Location: `include/tennis_watch/core/point.hpp`
   - Purpose: Define `struct Point` representing a single row of primitive tennis point data (match_id, PointNumber, PointWinner, PointServer, score state, break point flags).

3. **Step 3: Define CSV Parser Header (`csv_parser.hpp`)**
   - Location: `include/tennis_watch/ingestion/csv_parser.hpp`
   - Purpose: Declare the interface function/class, e.g., `std::vector<Point> parse_csv(const std::string& filepath)`.

4. **Step 4: Implement CSV Parser (`csv_parser.cpp`)**
   - Location: `src/ingestion/csv_parser.cpp`
   - Purpose: Open CSV via `std::ifstream`, read line-by-line (`std::getline`), parse/tokenise fields, convert types, and populate `std::vector<Point>`.

5. **Step 5: Create Verification Main (`main.cpp`)**
   - Location: `src/main.cpp`
   - Purpose: Instantiate parser on sample dataset `data/ausopen_2024_final.csv`, iterate over parsed points, and print output to stdout for Stage 1 sanity check.

6. **Step 6: Configure Build System (`CMakeLists.txt`)**
   - Location: `CMakeLists.txt`
   - Purpose: Setup CMake executable target linking `src/main.cpp` and `src/ingestion/csv_parser.cpp` with header include paths `include/`.

#### What to do with the CSV data:
- Analysis of information to include:
    - match_id
    - ElapsedTime - time, at the beginning of this point, since beginning of first point
    - SetNo
    - P1GamesWon - games won at the start of this point
    - SetWinner - 1 or 2 on the converted set point, otherwise 0
    - GameNo - game number of current point (initially 1)
    - GameWinner - 1 or 2 on the converted game point, otherwise 0
    - PointNumber - point number since the start of the match
    - PointWinner - winner of this point
    - PointServer - 
    - Speed_KMH 
    - Rally - length of the rally: not including the serve, 0 if double fault, 1 if ace, includes UFE, FE, winner
    - P1Score - score (0, 15, 30, 40, AD) after the point was played
    - P1PointsWon - cumulative points won in the match after this point was played
    - P1Ace - 1 for yes, 0 for no
    - P1Winner - 1 for yes, 0 for no
    - P1DoubleFault - 1 for yes, 0 for no etc.
    - P1UnfErr
    - P1NetPoint - 1 for yes they initiated the net point. Whether they won the net point can be calculated using PointWinner
    - P1BreakPoint
    - P1BreakPointWon
    - P1BreakPointMissed
    - P1FirstSrvIn
    - P1FirstSrvWon
    - P2FirstSrvWon
    - P1SecondSrvIn
    - P1SecondSrvWon
    - P1ForcedError

---

## Refactoring Plan: Core Domain Isolation

### Problem Identified
- `csv_parser.hpp` was initially placed inside `include/tennis_watch/core/`.
- File parsing is an I/O operation (Ingestion), whereas `core/` must strictly hold pure domain models and math/metric engines with **Zero I/O dependencies**.

### Correct Folder Assignments
1. **`include/tennis_watch/core/`**:
   - `point.hpp` (Pure domain struct holding raw point attributes).
   - `momentum.hpp` (Stage 3: Pure momentum calculation state engine).
   - `metrics.hpp` (Stage 3: Pure statistical metrics calculations).
2. **`include/tennis_watch/ingestion/`**:
   - `csv_parser.hpp` (File reading and line tokenization interface).
   - `replay_engine.hpp` (Stage 2: Timed point feed replay loop).

### Concrete Steps to Achieve Domain Isolation
1. **Move Header**: Relocate `csv_parser.hpp` from `include/tennis_watch/core/` to `include/tennis_watch/ingestion/csv_parser.hpp`.
2. **Isolate `point.hpp`**: Keep `point.hpp` in `include/tennis_watch/core/point.hpp` with zero I/O includes (only standard types like `std::string`, `int`).
3. **Include Bridge**: In `include/tennis_watch/ingestion/csv_parser.hpp`, include `#include "tennis_watch/core/point.hpp"`.
4. **Update `src/main.cpp`**: Change `#include "tennis_watch/core/csv_parser.hpp"` to `#include "tennis_watch/ingestion/csv_parser.hpp"`.

---

*2026.08.09*

# Execution Guide: Core Domain Isolation (`include/tennis_watch/core/`)

## Objective & Rules
This guide details how a C++ beginner can construct the `core/` domain layer step-by-step.
- **Zero I/O Rule**: Code inside `include/tennis_watch/core/` must **never** contain file operations (`std::ifstream`), networking (`sockets`), database calls, or stdout printing (`std::cout`).
- **Input/Output**: Functions and classes inside `core/` accept data structures (e.g. `Point`) and return calculated metric values (e.g., `double` momentum scores, statistical structs).

---

## Detailed Step-by-Step Breakdown for Beginners

### Step 1: Design the Primitive Domain Struct (`include/tennis_watch/core/point.hpp`)
1. **Understand standard C++ types & custom `struct`**:
   - In C++, a `struct` is a light object holding public fields.
   - Use `#pragma once` at the top of header files to prevent duplicate inclusion compilation errors.
   - Use standard types: `int` for numeric counts, `std::string` for match IDs / text scores, `bool` for true/false flags.
2. **Define Point Outcome Enums (Strongly Typed)**:
   - Define an `enum class PointOutcome` to represent how a point ended: `Ace`, `DoubleFault`, `Winner`, `UnforcedError`, `ForcedError`, `Routine`.
   - Using `enum class` prevents typos and ensures safe type checking in C++.
3. **Field Categorization in `Point` struct**:
   - **Match Context**: `match_id` (`std::string`), `point_number` (`int`), `set_number` (`int`), `game_number` (`int`).
   - **Scoreboard State**: `server` (`int`), `p1_score` (`std::string`), `p2_score` (`std::string`), `p1_games_won` (`int`), `p2_games_won` (`int`).
   - **Point Outcome & Detail**: `point_winner` (`int`), `outcome` (`PointOutcome`), `rally_length` (`int`), `serve_speed_kmh` (`double`).
   - **Break Point & Key Point Attributes**: `is_break_point` (`bool`), `break_point_player` (`int`), `is_break_point_converted` (`bool`).

### Step 2: Establish In-Memory Rolling State (`std::deque<Point>`)
1. **Why `std::deque` (Double-ended Queue)?**:
   - When calculating rolling metrics (e.g., "last 10 points"), we want to easily push a new point onto the back (`push_back()`) and remove the oldest point from the front (`pop_front()`) when the window size exceeds $N$.
   - Unlike `std::vector`, `std::deque` makes front removals extremely efficient ($O(1)$ time complexity).
2. **Usage Pattern**:
   - Maintain a window size limit (e.g. `size_t window_size = 10;`).
   - Upon receiving a new `Point`, append it to `std::deque<Point>`. If `deque.size() > window_size`, call `deque.pop_front()`.

### Step 3: Implement Pure Calculation Engines
1. **`MomentumEngine` (`include/tennis_watch/core/momentum.hpp`)**:
   - **Class Responsibility**: Manages the rolling deque of points and evaluates player momentum dynamically.
   - **Weighting Mechanics**: Break points, aces, and winners add higher momentum weight than standard unforced errors or routine points.
   - **Zero I/O**: Exposes clean getter methods like `double calculate_p1_momentum() const` without printing to console.
2. **`MetricsEngine` (`include/tennis_watch/core/metrics.hpp`)**:
   - **Class Responsibility**: Computes cumulative statistical ratios over the match or sliding windows.
   - **Derived Stats**:
     - Break point conversion rate: `(break_points_won / break_points_faced) * 100.0`.
     - Serve dominance rate: percentage of points won on first serve vs. second serve.
     - Error ratios: unforced errors vs. forced errors per set/match.

### Step 4: Validate via Isolated Unit Testing (`tests/unit/test_metrics.cpp`)
1. **Zero Dependency Testing**:
   - Instantiate a `MomentumEngine` object directly in code.
   - Construct dummy/synthetic `Point` structs manually in code (no CSV reading required).
   - Feed synthetic points into the engine and verify calculated values using assertions (`assert()`).

---

*2026.08.09*

# Simplified MVP Execution Plan (Stages 1 & 2: Parser + Replay CLI)

## Goal
Build a working Minimum Viable Product (MVP) CLI tool that ingests point data from CSV and replays match progression in real-time to standard output (`stdout`), without any analytics engines or metric calculations for now.

---

## 4-Step MVP Execution Roadmap

### Step 1: Define Primitive Data Struct (`include/tennis_watch/core/point.hpp`)
Keep `Point` lightweight and focused strictly on scoreboard state and outcome descriptors:
- `point_number` (`int`)
- `set_number` (`int`)
- `game_number` (`int`)
- `p1_score` (`std::string`) / `p2_score` (`std::string`)
- `point_winner` (`int` — `1` for Player 1, `2` for Player 2)
- `point_server` (`int`)
- `outcome_desc` (`std::string` — e.g. "Ace", "Double Fault", "Winner", "Unforced Error", "Forced Error", "Routine")
- `is_break_point` (`bool`)

### Step 2: Implement CSV Parser (`include/tennis_watch/ingestion/csv_parser.hpp` & `src/ingestion/csv_parser.cpp`)
- **Functionality**: Read `data/sample.csv` line by line using `std::ifstream` and `std::getline`.
- **Parsing**: Split row by commas, map key CSV columns (PointNumber, SetNo, GameNo, P1Score, P2Score, PointWinner, PointServer, outcome flags) into `Point` instances.
- **Output**: Return a `std::vector<Point>` containing all points in the match.

### Step 3: Implement Timed Replay Loop (`include/tennis_watch/ingestion/replay_engine.hpp` & `src/ingestion/replay_engine.cpp`)
- **Functionality**: Iterate through `std::vector<Point>`.
- **Console Output**: Print formatted point updates to `stdout`, e.g.:
  `[POINT #14] Player 1 wins (Ace) | Set 1, Game 2 | Score: 30-15`
  `[BREAK POINT] Player 2 serving | 40-AD`
- **Simulated Real-Time**: Add `std::this_thread::sleep_for(std::chrono::milliseconds(500))` between points.

### Step 4: CLI Entry Point & Build (`src/main.cpp` & `CMakeLists.txt`)
- In `main.cpp`, parse CSV filepath argument (default: `data/sample.csv`).
- Trigger parser -> get `std::vector<Point>` -> invoke replay loop.
- Compile with CMake and verify execution with `./tennis_watch`.