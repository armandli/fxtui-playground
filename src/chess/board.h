#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "piece.h"

struct CastlingRights {
    bool white_kingside = true;
    bool white_queenside = true;
    bool black_kingside = true;
    bool black_queenside = true;
};

struct Move {
    Position from;
    Position to;
    std::optional<PieceType> promotion;  // Queen/Rook/Bishop/Knight, else nullopt
    bool is_en_passant = false;
    // Castling is detected structurally (king moving 2 files) -- no explicit flag needed.
};

inline bool operator==(const Move& a, const Move& b) {
    return a.from == b.from && a.to == b.to && a.promotion == b.promotion &&
           a.is_en_passant == b.is_en_passant;
}

struct GameState {
    Side side_to_move = Side::White;
    CastlingRights castling;
    std::optional<Position> en_passant_target;
    int halfmove_clock = 0;
    int fullmove_number = 1;
    std::vector<std::string> moves_uci;             // for "position startpos moves ..."
    std::map<std::string, int> position_history;    // PositionKey -> occurrence count
};

// Indexed [rank][file].
using Board = std::array<std::array<std::optional<Piece>, 8>, 8>;

inline Board InitialBoard() {
    Board b{};
    auto put = [&](int file, int rank, PieceType t, Side s) {
        b[rank][file] = Piece{t, s};
    };
    const PieceType back[8] = {
        PieceType::Rook, PieceType::Knight, PieceType::Bishop, PieceType::Queen,
        PieceType::King, PieceType::Bishop, PieceType::Knight, PieceType::Rook,
    };
    for (int f = 0; f < 8; ++f) {
        put(f, 0, back[f], Side::White);
        put(f, 1, PieceType::Pawn, Side::White);
        put(f, 6, PieceType::Pawn, Side::Black);
        put(f, 7, back[f], Side::Black);
    }
    return b;
}

inline std::string UciMoveString(const Move& m) {
    std::string s = SquareName(m.from) + SquareName(m.to);
    if (m.promotion) {
        switch (*m.promotion) {
            case PieceType::Queen:  s += 'q'; break;
            case PieceType::Rook:   s += 'r'; break;
            case PieceType::Bishop: s += 'b'; break;
            case PieceType::Knight: s += 'n'; break;
            default: break;
        }
    }
    return s;
}

// Default board coloring per the Wikipedia chess page convention: "each
// player has a light square at their near right-hand corner" -- a1 is dark.
inline bool IsDarkSquare(Position p) {
    return (p.file + p.rank) % 2 == 0;
}
