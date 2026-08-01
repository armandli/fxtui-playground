#ifndef UCI_PROTOCOL_H
#define UCI_PROTOCOL_H

// Pure UCI command builders and response parsers -- no I/O or process
// knowledge. Commands are returned without a trailing newline; the transport
// (stockfish_client.h) owns framing/flushing.
//
// Spec: https://backscattering.de/chess/uci/
//       https://official-stockfish.github.io/docs/stockfish-wiki/UCI-Protocol-and-Stockfish-Commands.html

#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline std::string cmd_uci() { return "uci"; }
inline std::string cmd_is_ready() { return "isready"; }
inline std::string cmd_uci_new_game() { return "ucinewgame"; }
inline std::string cmd_stop() { return "stop"; }
inline std::string cmd_quit() { return "quit"; }

inline std::string cmd_set_option(
    const std::string& name, const std::string& value)
{
  return "setoption name " + name + " value " + value;
}

inline std::string cmd_position_startpos(
    const std::vector<std::string>& moves)
{
  std::string s = "position startpos";
  if (not moves.empty()) {
    s += " moves";
    for (auto& m : moves)
      s += " " + m;
  }
  return s;
}

inline std::string cmd_go_movetime(int ms) {
  return "go movetime " + std::to_string(ms);
}
inline std::string cmd_go_depth(int depth) {
  return "go depth " + std::to_string(depth);
}

inline bool is_uci_ok(std::string_view line) { return line == "uciok"; }
inline bool is_ready_ok(std::string_view line) { return line == "readyok"; }

struct BestMove {
  std::string move;
  std::optional<std::string> ponder;
};

// Returns nullopt for non-"bestmove" lines, and also for "bestmove (none)" /
// an empty move token -- the caller treats that as an engine/move-generator
// disagreement rather than a silent no-op (it should never happen for a
// correct generator, since we stop querying once we detect game-over).
inline std::optional<BestMove> parse_best_move(std::string_view line) {
  const std::string_view prefix = "bestmove ";
  if (line.rfind(prefix, 0) != 0)
    return std::nullopt;
  std::string_view rest = line.substr(prefix.size());
  size_t sp = rest.find(' ');
  std::string_view move =
      (sp == std::string_view::npos) ? rest : rest.substr(0, sp);
  if (move.empty() or move == "(none)")
    return std::nullopt;

  BestMove bm;
  bm.move = move;
  if (sp != std::string_view::npos) {
    std::string_view tail = rest.substr(sp + 1);
    const std::string_view ponder_prefix = "ponder ";
    if (tail.rfind(ponder_prefix, 0) == 0)
      bm.ponder = tail.substr(ponder_prefix.size());
  }
  return bm;
}

#endif  // UCI_PROTOCOL_H
