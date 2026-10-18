#!/usr/bin/env bash
# Plays a match between chess-engine and Stockfish limited to a given Elo, using fastchess
# (https://github.com/Disservin/fastchess). Each opening is played with both colors.
#
#   scripts/match.sh <stockfish_elo> [rounds] [tc] [concurrency]
#
# Environment overrides: ENGINE (path to chess-engine), STOCKFISH, FASTCHESS.
set -euo pipefail

ELO=${1:?usage: match.sh <stockfish_elo> [rounds] [tc] [concurrency]}
ROUNDS=${2:-100}
TC=${3:-10+0.1}
CONCURRENCY=${4:-2}

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${ENGINE:-$ROOT/build/chess-engine}
STOCKFISH=${STOCKFISH:-$(command -v stockfish || echo /usr/games/stockfish)}
FASTCHESS=${FASTCHESS:-fastchess}
OUT=${OUT:-$ROOT/matches}
mkdir -p "$OUT"

"$FASTCHESS" \
  -engine cmd="$ENGINE" name=chess-engine \
  -engine cmd="$STOCKFISH" name="stockfish-$ELO" option.UCI_LimitStrength=true option.UCI_Elo="$ELO" \
  -each tc="$TC" option.Hash=16 \
  -openings file="$ROOT/scripts/openings.epd" format=epd order=random \
  -rounds "$ROUNDS" -games 2 -repeat \
  -concurrency "$CONCURRENCY" \
  -recover \
  -pgnout file="$OUT/vs-sf$ELO.pgn" \
  2>&1 | tee "$OUT/vs-sf$ELO.log"
