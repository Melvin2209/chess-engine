#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <thread>

#include "position.h"
#include "search.h"
#include "tt.h"

namespace chess {

constexpr const char* ENGINE_NAME = "chess-engine";
constexpr const char* ENGINE_VERSION = "1.0";
constexpr const char* ENGINE_AUTHOR = "Melvin";

// Universal Chess Interface front end. Searches run on a worker thread so that
// "stop" and "isready" are answered while the engine thinks.
class UciEngine {
public:
    UciEngine(std::istream& in, std::ostream& out);
    ~UciEngine();

    void loop();
    // Executes one command line; returns false on "quit".
    bool handle_command(const std::string& line);

private:
    void cmd_uci();
    void cmd_setoption(std::istream& args);
    void cmd_position(std::istream& args);
    void cmd_go(std::istream& args);
    void cmd_perft(std::istream& args);
    void wait_for_search();

    std::istream& in_;
    std::ostream& out_;
    Position pos_;
    TranspositionTable tt_;
    Searcher searcher_;
    std::thread search_thread_;
    std::int64_t move_overhead_ = 30;
};

}  // namespace chess
