#ifndef PIECE_H
#define PIECE_H

#include <cassert>

#include <optional>
#include <string>
#include <string_view>

// Not named `Color` on purpose: ftxui::Color is a class declared directly in
// namespace ftxui with static members `White`/`Black`. Every file here does
// `using namespace ftxui;`, and a same-named enum declared directly in the
// global namespace would hide ftxui::Color for unqualified lookup, breaking
// every `Color::RGB(...)` call in main.cpp.
enum class Side : int { White, Black };

inline Side opposite(Side s) {
  return s == Side::White ? Side::Black : Side::White;
}

enum class PieceType : int { Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
  PieceType type;
  Side side;
};

inline bool operator==(const Piece& a, const Piece& b) {
  return a.type == b.type and a.side == b.side;
}

struct Position {
  int file = 0;  // 0=a .. 7=h
  int rank = 0;  // 0=rank1 .. 7=rank8
};

inline bool operator==(const Position& a, const Position& b) {
  return a.file == b.file and a.rank == b.rank;
}

inline bool in_bounds(Position p) {
  return p.file >= 0 and p.file < 8 and p.rank >= 0 and p.rank < 8;
}

inline std::string square_name(Position p) {
  return std::string(1, char('a' + p.file)) +
      std::string(1, char('1' + p.rank));
}

inline std::optional<Position> parse_square(std::string_view s) {
  if (s.size() != 2)
    return std::nullopt;
  int f = s[0] - 'a';
  int r = s[1] - '1';
  if (f < 0 or f > 7 or r < 0 or r > 7)
    return std::nullopt;
  return Position{f, r};
}

// Filled glyphs (U+265A-265F) are used for BOTH sides, differentiated only by
// foreground color: the hollow "white" chess glyphs (U+2654-2659) have poor
// coverage in many terminal fonts and often render as tofu boxes.
inline std::string piece_glyph(PieceType t) {
  switch (t) {
    case PieceType::King:
      return "♚";

    break; case PieceType::Queen:
      return "♛";

    break; case PieceType::Rook:
      return "♜";

    break; case PieceType::Bishop:
      return "♝";

    break; case PieceType::Knight:
      return "♞";

    break; case PieceType::Pawn:
      return "♟";

    break; default:
      assert(false);  // should never get here
      return "?";
  }
}

#endif  // PIECE_H
