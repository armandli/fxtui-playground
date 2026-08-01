#pragma once

// Pure UCI command builders and response parsers -- no I/O or process
// knowledge. Commands are returned without a trailing newline; the transport
// (stockfish_client.h) owns framing/flushing.
//
// Spec: https://backscattering.de/chess/uci/
//       https://official-stockfish.github.io/docs/stockfish-wiki/UCI-Protocol-and-Stockfish-Commands.html

#include <optional>
#include <string>
#include <vector>

inline std::string CmdUci() { return "uci"; }
inline std::string CmdIsReady() { return "isready"; }
inline std::string CmdUciNewGame() { return "ucinewgame"; }
inline std::string CmdStop() { return "stop"; }
inline std::string CmdQuit() { return "quit"; }

inline std::string CmdSetOption(const std::string& name, const std::string& value) {
    return "setoption name " + name + " value " + value;
}

inline std::string CmdPositionStartpos(const std::vector<std::string>& moves) {
    std::string s = "position startpos";
    if (!moves.empty()) {
        s += " moves";
        for (auto& m : moves)
            s += " " + m;
    }
    return s;
}

inline std::string CmdGoMovetime(int ms) { return "go movetime " + std::to_string(ms); }
inline std::string CmdGoDepth(int depth) { return "go depth " + std::to_string(depth); }

inline bool IsUciOk(const std::string& line) { return line == "uciok"; }
inline bool IsReadyOk(const std::string& line) { return line == "readyok"; }

struct BestMove {
    std::string move;
    std::optional<std::string> ponder;
};

// Returns nullopt for non-"bestmove" lines, and also for "bestmove (none)" /
// an empty move token -- the caller treats that as an engine/move-generator
// disagreement rather than a silent no-op (it should never happen for a
// correct generator, since we stop querying once we detect game-over).
inline std::optional<BestMove> ParseBestMove(const std::string& line) {
    const std::string prefix = "bestmove ";
    if (line.rfind(prefix, 0) != 0)
        return std::nullopt;
    std::string rest = line.substr(prefix.size());
    size_t sp = rest.find(' ');
    std::string move = (sp == std::string::npos) ? rest : rest.substr(0, sp);
    if (move.empty() || move == "(none)")
        return std::nullopt;

    BestMove bm;
    bm.move = move;
    if (sp != std::string::npos) {
        std::string tail = rest.substr(sp + 1);
        const std::string ponder_prefix = "ponder ";
        if (tail.rfind(ponder_prefix, 0) == 0)
            bm.ponder = tail.substr(ponder_prefix.size());
    }
    return bm;
}
