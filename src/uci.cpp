#include "uci.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>

#include "bench.h"
#include "eval.h"

namespace chess {

namespace {

// Output from the search thread and the input thread must not interleave mid-line.
std::mutex output_mutex;

void send(std::ostream& out, const std::string& line) {
    std::lock_guard lock(output_mutex);
    out << line << std::endl;
}

std::string format_info(const SearchInfo& info) {
    std::ostringstream ss;
    const std::int64_t nps = info.time_ms > 0 ? std::int64_t(info.nodes * 1000 / info.time_ms) : 0;
    ss << "info depth " << info.depth << " seldepth " << info.seldepth << " score " << uci_score(info.score)
       << " nodes " << info.nodes << " nps " << nps << " hashfull " << info.hashfull << " time " << info.time_ms
       << " pv";
    for (Move m : info.pv) ss << ' ' << m.to_uci();
    return ss.str();
}

}  // namespace

UciEngine::UciEngine(std::istream& in, std::ostream& out) : in_(in), out_(out), tt_(16), searcher_(tt_) {}

UciEngine::~UciEngine() {
    searcher_.stop();
    wait_for_search();
}

void UciEngine::loop() {
    std::string line;
    while (std::getline(in_, line))
        if (!handle_command(line)) break;
    searcher_.stop();
    wait_for_search();
}

void UciEngine::wait_for_search() {
    if (search_thread_.joinable()) search_thread_.join();
}

bool UciEngine::handle_command(const std::string& line) {
    std::istringstream args(line);
    std::string cmd;
    if (!(args >> cmd)) return true;

    if (cmd == "uci") {
        cmd_uci();
    } else if (cmd == "isready") {
        send(out_, "readyok");
    } else if (cmd == "setoption") {
        wait_for_search();
        cmd_setoption(args);
    } else if (cmd == "ucinewgame") {
        searcher_.stop();
        wait_for_search();
        tt_.clear();
        searcher_.clear_history();
    } else if (cmd == "position") {
        wait_for_search();
        cmd_position(args);
    } else if (cmd == "go") {
        searcher_.stop();
        wait_for_search();
        cmd_go(args);
    } else if (cmd == "stop") {
        searcher_.stop();
        wait_for_search();
    } else if (cmd == "ponderhit") {
        // Pondering is not supported; nothing to do.
    } else if (cmd == "quit") {
        return false;
    } else if (cmd == "d") {
        send(out_, pos_.pretty());
    } else if (cmd == "eval") {
        send(out_, eval_trace(pos_));
    } else if (cmd == "perft") {
        wait_for_search();
        cmd_perft(args);
    } else if (cmd == "bench") {
        wait_for_search();
        int depth = 0;
        args >> depth;
        run_bench(out_, depth > 0 ? depth : DEFAULT_BENCH_DEPTH);
    } else {
        send(out_, "info string unknown command: " + line);
    }
    return true;
}

void UciEngine::cmd_uci() {
    std::ostringstream ss;
    ss << "id name " << ENGINE_NAME << ' ' << ENGINE_VERSION << '\n'
       << "id author " << ENGINE_AUTHOR << '\n'
       << "option name Hash type spin default 16 min 1 max 4096\n"
       << "option name Clear Hash type button\n"
       << "option name Move Overhead type spin default 30 min 0 max 5000\n"
       << "uciok";
    send(out_, ss.str());
}

void UciEngine::cmd_setoption(std::istream& args) {
    std::string token, name, value;
    args >> token;  // "name"
    while (args >> token && token != "value") name += (name.empty() ? "" : " ") + token;
    std::getline(args >> std::ws, value);

    if (name == "Hash") {
        tt_.resize(std::stoul(value));
    } else if (name == "Clear Hash") {
        tt_.clear();
    } else if (name == "Move Overhead") {
        move_overhead_ = std::stoll(value);
    } else {
        send(out_, "info string unknown option: " + name);
    }
}

void UciEngine::cmd_position(std::istream& args) {
    std::string token, fen;
    args >> token;
    if (token == "startpos") {
        fen = std::string(Position::START_FEN);
        args >> token;  // optional "moves"
    } else if (token == "fen") {
        while (args >> token && token != "moves") fen += token + ' ';
    } else {
        return;
    }

    try {
        pos_.set_fen(fen);
    } catch (const std::exception& e) {
        send(out_, std::string("info string invalid fen: ") + e.what());
        pos_.set_fen(Position::START_FEN);
        return;
    }

    while (args >> token) {
        const Move m = pos_.parse_uci_move(token);
        if (!m) {
            send(out_, "info string illegal move: " + token);
            break;
        }
        pos_.make_move(m);
    }
}

void UciEngine::cmd_go(std::istream& args) {
    SearchLimits limits;
    limits.move_overhead = move_overhead_;
    std::string token;
    int perft_depth = 0;
    while (args >> token) {
        if (token == "wtime") args >> limits.time[WHITE];
        else if (token == "btime") args >> limits.time[BLACK];
        else if (token == "winc") args >> limits.inc[WHITE];
        else if (token == "binc") args >> limits.inc[BLACK];
        else if (token == "movestogo") args >> limits.movestogo;
        else if (token == "depth") args >> limits.depth;
        else if (token == "nodes") args >> limits.nodes;
        else if (token == "movetime") args >> limits.movetime;
        else if (token == "infinite") limits.infinite = true;
        else if (token == "perft") args >> perft_depth;
    }
    if (perft_depth > 0) {
        std::istringstream perft_args(std::to_string(perft_depth));
        cmd_perft(perft_args);
        return;
    }

    search_thread_ = std::thread([this, limits, pos = pos_] {
        const SearchResult result =
            searcher_.search(pos, limits, [this](const SearchInfo& info) { send(out_, format_info(info)); });
        std::string line = "bestmove " + result.best_move.to_uci();
        if (result.ponder_move) line += " ponder " + result.ponder_move.to_uci();
        send(out_, line);
    });
}

void UciEngine::cmd_perft(std::istream& args) {
    int depth = 1;
    args >> depth;
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t total = 0;
    std::ostringstream ss;
    // "Divide" output: node count below each root move, handy for debugging against other engines.
    for (Move m : legal_moves(pos_)) {
        pos_.make_move(m);
        const std::uint64_t n = depth > 1 ? perft(pos_, depth - 1) : 1;
        pos_.unmake_move(m);
        total += n;
        ss << m.to_uci() << ": " << n << '\n';
    }
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    ss << "\nNodes searched: " << total << "\nTime: " << ms << " ms\nNPS: " << (ms ? total * 1000 / ms : 0);
    send(out_, ss.str());
}

}  // namespace chess
