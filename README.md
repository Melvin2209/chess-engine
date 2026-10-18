# chess-engine

A UCI chess engine written in modern C++20: bitboard board representation, fully legal move
generation validated by perft, alpha-beta search with a transposition table, and a hand-crafted
tapered evaluation. It plays in any UCI GUI (Cute Chess, Arena, BanksiaGUI, ...) and online as a
lichess bot.

| | |
|---|---|
| **Estimated strength** | **≈ 2500 Elo** at 10s+0.1s, measured against Elo-limited Stockfish 16 ([details](#strength-estimate)) |
| **Search speed** | ≈ 1.9 M nodes/s (single thread, `bench`) |
| **Perft speed** | ≈ 200 M nodes/s (startpos depth 6 in 603 ms) |
| **Tests** | 67 GoogleTest cases, including 21 perft positions (+6 deep perft runs) |

## Features

**Board representation**
- 64-bit bitboards per piece type and color, plus a mailbox array for O(1) piece lookup
- Magic bitboards for sliding pieces (magics found at startup with a deterministic PRNG)
- Precomputed `between`/`line` tables for pins and check evasions
- Incremental Zobrist hashing (position key and pawn key); the en passant file is only hashed
  when a capture is actually possible, so transpositions hash identically

**Move generation**
- Strictly legal generation (no make/unmake filtering): check evasions, double checks, absolute
  pins, castling through attacked squares, en passant discovered checks, all promotions
- Separate generator for "noisy" moves (captures and queen promotions) used by quiescence search
- Validated with perft on the standard [CPW reference positions](https://www.chessprogramming.org/Perft_Results)
  and 14 additional edge-case positions

**Search**
- Negamax alpha-beta with principal variation search (PVS)
- Iterative deepening with aspiration windows
- Transposition table (16-byte entries, depth/age-aware replacement, mate scores stored relative to the node)
- Move ordering: hash move → good captures (MVV-LVA, filtered by static exchange evaluation) →
  killer moves → history heuristic → losing captures
- Quiescence search with stand-pat, delta pruning and SEE pruning; check evasions in quiescence
- Selectivity: null-move pruning, reverse futility pruning, futility pruning, late move pruning,
  late move reductions, internal iterative reductions, check extensions, mate distance pruning
- Draw detection: threefold repetition, fifty-move rule, insufficient material
- Time management for `wtime/btime/winc/binc/movestogo`, `movetime`, `depth`, `nodes`, `infinite`

**Evaluation** (tapered between middlegame and endgame by remaining material)
- Material
- Piece-square tables (separate middlegame/endgame tables for pawns and king)
- Pawn structure: doubled, isolated and passed pawns (bonus scaled by rank)
- Bishop pair and tempo bonus

## Building

Requirements: a C++20 compiler (GCC ≥ 10, Clang ≥ 12) and CMake ≥ 3.20. GoogleTest is taken from
the system if installed (`libgtest-dev`), otherwise downloaded automatically with `FetchContent`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary is `build/chess-engine`. Options:

| CMake option | Default | Description |
|---|---|---|
| `ENGINE_NATIVE` | `ON` | Compile with `-march=native` (fast `popcnt`/`tzcnt`); turn off for portable binaries |
| `ENGINE_BUILD_TESTS` | `ON` | Build the GoogleTest suite |

## Usage

The engine speaks [UCI](https://www.wbec-ridderkerk.nl/html/UCIProtocol.html). To play against it,
add `build/chess-engine` as a UCI engine in your GUI (in Cute Chess: *Tools → Settings → Engines → +*).

UCI options:

| Option | Type | Default | Description |
|---|---|---|---|
| `Hash` | spin | 16 | Transposition table size in MB (1–4096) |
| `Clear Hash` | button | | Empties the transposition table |
| `Move Overhead` | spin | 30 | Time in ms kept in reserve per move for GUI/network latency |

Besides the standard commands, the engine understands a few debugging commands:

```text
d                 print the board, FEN and Zobrist key
eval              print the evaluation broken down by term
perft <depth>     count leaf nodes, with per-move breakdown ("divide")
go perft <depth>  same as above
bench [depth]     run the benchmark (also: ./chess-engine bench [depth])
```

Example session:

```text
$ ./build/chess-engine
uci
id name chess-engine 1.0
...
uciok
position startpos moves e2e4 e7e5
go movetime 1000
...
info depth 17 seldepth 26 score cp 15 nodes 806737 nps 1871779 hashfull 183 time 431 pv g1f3 g8f6 f3e5 d7d6 ...
info depth 18 seldepth 28 score cp 20 nodes 1201062 nps 1906447 hashfull 282 time 630 pv g1f3 g8f6 f3e5 d7d6 ...
bestmove g1f3 ponder g8f6
```

## Testing

```bash
ctest --test-dir build --output-on-failure
# or directly, including the slow perft runs (≈1.45 billion nodes):
./build/engine_tests --gtest_also_run_disabled_tests
```

| Suite | What it checks |
|---|---|
| `Perft` | Node counts for 7 reference positions and 14 edge cases (en passant pins, castling rights, under-promotions, double check, ...). The `DISABLED_Deep` suite goes to depth 5–7 (e.g. startpos depth 6 = 119,060,324, Kiwipete depth 5 = 193,690,690) |
| `Position` | FEN parsing/printing, make/unmake, incremental vs. full Zobrist keys over random games, repetition, fifty-move rule, insufficient material, static exchange evaluation |
| `MoveGen` | Mate/stalemate, castling legality, en passant pins, noisy generator ⊆ full generator |
| `Eval` | Color symmetry over random positions (`eval(pos) == eval(mirror(pos))`), pawn-structure terms |
| `Search` | Mate in 1/2, being mated, avoiding stalemate, legal PV, determinism, time and node limits |

CI (GitHub Actions) builds and tests on Linux and macOS in Release and Debug, and runs the suite
again under AddressSanitizer and UndefinedBehaviorSanitizer.

### Perft results

Measured on an Intel Xeon @ 2.10 GHz (cloud VM, 4 vCPUs) (single thread, bulk counting at the leaves):

| Position | Depth | Nodes |
|---|---|---|
| Start position | 6 | 119,060,324 |
| Kiwipete | 5 | 193,690,690 |
| Position 3 | 7 | 178,633,661 |
| Position 4 | 6 | 706,045,033 |
| Position 5 | 5 | 89,941,194 |
| Position 6 | 5 | 164,075,551 |

All six deep runs (≈1.45 billion nodes in total) complete in about 6.5 s, i.e. ≈220 M nodes/s;
`perft 6` from the start position takes 603 ms.

## Benchmark

`./build/chess-engine bench` searches 12 fixed positions to depth 13, each with a fresh hash table.
The node count is deterministic and acts as a signature of the search: a change that is
meant to be a pure speedup must not change it.

```text
$ ./build/chess-engine bench
Position  1: bestmove  ...
...
===========================
Total time (ms) : 818
Nodes searched  : 1544232
Nodes/second    : 1887814
```

## Strength estimate

The Elo was estimated by playing matches with [fastchess](https://github.com/Disservin/fastchess)
against **Stockfish 16 with `UCI_LimitStrength`**, whose `UCI_Elo` setting is calibrated by the
Stockfish team against the CCRL rating list. The opponent's level was then adjusted until the score
was close to 50%.

- Time control: 10s + 0.1s per move, 1 thread, 16 MB hash for both engines
- 44 opening positions (`scripts/openings.epd`), each played with both colors
- Hardware: an Intel Xeon @ 2.10 GHz (cloud VM, 4 vCPUs)

| Opponent | Games | W / D / L | Score | Elo difference | Performance |
|---|---|---|---|---|---|
| Stockfish 16, `UCI_Elo` 2400 | 200 | 111 / 13 / 76 | 58.8% | +61 ± 49 | ≈ 2460 |
| Stockfish 16, `UCI_Elo` 2600 | 200 | 74 / 24 / 102 | 43.0% | −49 ± 43 | ≈ 2550 |

Shorter preliminary runs: 95% against `UCI_Elo` 1320 (20 games) and 82% against `UCI_Elo` 2000 (30 games).
All 400 games of the main matches ended normally (no time losses, crashes or illegal moves).

**Estimate: about 2500 Elo on the CCRL scale** (the two matches give 2460 and 2550). Treat this as a rough figure:

- the error bars are wide (± 45–50 Elo at 95% for each 200-game match, and the two results do not fully agree),
- Stockfish's strength limiter is calibrated at longer time controls and plays deliberately weak
  moves at random, which is not the same as a genuinely weaker engine,
- a real rating requires a gauntlet against several engines of similar strength.

To reproduce (requires `fastchess` and `stockfish` in `PATH`):

```bash
scripts/match.sh 2400 100 10+0.1 4   # <stockfish Elo> <rounds> <time control> <concurrency>
```

## Playing on lichess

See [docs/lichess.md](docs/lichess.md): create a bot account, then run the engine through
[lichess-bot](https://github.com/lichess-bot-devs/lichess-bot) with the provided
[configuration excerpt](docs/lichess-bot.config.yml).

## Project layout

```text
src/
  types.h           core types: squares, pieces, 16-bit move encoding, scores
  bitboard.h/.cpp   bitboard helpers, attack tables, magic bitboards
  position.h/.cpp   board state, FEN, make/unmake, Zobrist hashing, SEE, draw detection
  movegen.h/.cpp    legal move generation, perft
  eval.h/.cpp       tapered evaluation (material, PST, pawn structure)
  tt.h/.cpp         transposition table
  search.h/.cpp     iterative deepening, alpha-beta/PVS, quiescence, time management
  uci.h/.cpp        UCI protocol, search thread
  bench.h/.cpp      fixed-depth benchmark
  main.cpp
tests/              GoogleTest suite
scripts/            match script and opening book for Elo estimation
docs/               lichess bot setup
```

## Possible improvements

- Evaluation: mobility, king safety, rook on open files, tuned weights (Texel tuning) or an NNUE network
- Search: staged move generation, counter-move and continuation history, singular extensions
- Lazy SMP multi-threading, pondering, Syzygy endgame tablebases

## License

[MIT](LICENSE)
