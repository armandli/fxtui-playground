#pragma once

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "board.h"
#include "piece.h"

inline bool IsSquareAttacked(const Board& board, Position sq, Side by) {
    // Pawns: a `by`-pawn attacks sq if it sits one rank "behind" sq (from its
    // own forward direction) and one file to either side.
    int pawn_dr = (by == Side::White) ? -1 : 1;
    for (int df : {-1, 1}) {
        Position p{sq.file + df, sq.rank + pawn_dr};
        if (InBounds(p) && board[p.rank][p.file] &&
            board[p.rank][p.file]->side == by && board[p.rank][p.file]->type == PieceType::Pawn)
            return true;
    }
    // Knights
    static const int kdf[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int kdr[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for (int i = 0; i < 8; ++i) {
        Position p{sq.file + kdf[i], sq.rank + kdr[i]};
        if (InBounds(p) && board[p.rank][p.file] &&
            board[p.rank][p.file]->side == by && board[p.rank][p.file]->type == PieceType::Knight)
            return true;
    }
    // King (adjacent)
    for (int df = -1; df <= 1; ++df)
        for (int dr = -1; dr <= 1; ++dr) {
            if (df == 0 && dr == 0)
                continue;
            Position p{sq.file + df, sq.rank + dr};
            if (InBounds(p) && board[p.rank][p.file] &&
                board[p.rank][p.file]->side == by && board[p.rank][p.file]->type == PieceType::King)
                return true;
        }
    // Sliding: rook/queen (orthogonal), bishop/queen (diagonal)
    static const int rdf[4] = {1, -1, 0, 0}, rdr[4] = {0, 0, 1, -1};
    static const int bdf[4] = {1, 1, -1, -1}, bdr[4] = {1, -1, 1, -1};
    auto scan = [&](const int* ddf, const int* ddr, PieceType a, PieceType b) {
        for (int i = 0; i < 4; ++i) {
            Position p{sq.file + ddf[i], sq.rank + ddr[i]};
            while (InBounds(p)) {
                if (board[p.rank][p.file]) {
                    auto& pc = *board[p.rank][p.file];
                    if (pc.side == by && (pc.type == a || pc.type == b))
                        return true;
                    break;  // blocked, stop this ray
                }
                p.file += ddf[i];
                p.rank += ddr[i];
            }
        }
        return false;
    };
    if (scan(rdf, rdr, PieceType::Rook, PieceType::Queen))
        return true;
    if (scan(bdf, bdr, PieceType::Bishop, PieceType::Queen))
        return true;
    return false;
}

inline Position FindKing(const Board& board, Side side) {
    for (int r = 0; r < 8; ++r)
        for (int f = 0; f < 8; ++f)
            if (board[r][f] && board[r][f]->type == PieceType::King && board[r][f]->side == side)
                return {f, r};
    return {-1, -1};  // should never happen in a legally-maintained game
}

namespace detail {

inline void AddPawnMoves(const Board& board, const GameState& state, Position from,
                          std::vector<Move>& out) {
    Side s = board[from.rank][from.file]->side;
    int dir = (s == Side::White) ? 1 : -1;
    int start_rank = (s == Side::White) ? 1 : 6;
    int promo_rank = (s == Side::White) ? 7 : 0;

    auto add_with_promotion = [&](Position to, bool en_passant) {
        if (to.rank == promo_rank) {
            for (PieceType pt : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight})
                out.push_back(Move{from, to, pt, en_passant});
        } else {
            out.push_back(Move{from, to, std::nullopt, en_passant});
        }
    };

    // Single push
    Position one{from.file, from.rank + dir};
    bool one_open = InBounds(one) && !board[one.rank][one.file];
    if (one_open)
        add_with_promotion(one, false);

    // Double push
    if (one_open && from.rank == start_rank) {
        Position two{from.file, from.rank + 2 * dir};
        if (InBounds(two) && !board[two.rank][two.file])
            out.push_back(Move{from, two, std::nullopt, false});
    }

    // Captures (including en passant)
    for (int df : {-1, 1}) {
        Position to{from.file + df, from.rank + dir};
        if (!InBounds(to))
            continue;
        if (board[to.rank][to.file] && board[to.rank][to.file]->side != s) {
            add_with_promotion(to, false);
        } else if (!board[to.rank][to.file] && state.en_passant_target && *state.en_passant_target == to) {
            // Confirm an enemy pawn is actually adjacent (same rank as `from`), rather than
            // trusting the stored target blindly.
            Position captured{to.file, from.rank};
            if (InBounds(captured) && board[captured.rank][captured.file] &&
                board[captured.rank][captured.file]->side != s &&
                board[captured.rank][captured.file]->type == PieceType::Pawn)
                out.push_back(Move{from, to, std::nullopt, true});
        }
    }
}

inline void AddKnightMoves(const Board& board, Position from, std::vector<Move>& out) {
    Side s = board[from.rank][from.file]->side;
    static const int kdf[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int kdr[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for (int i = 0; i < 8; ++i) {
        Position to{from.file + kdf[i], from.rank + kdr[i]};
        if (!InBounds(to))
            continue;
        if (!board[to.rank][to.file] || board[to.rank][to.file]->side != s)
            out.push_back(Move{from, to, std::nullopt, false});
    }
}

inline void AddKingStepMoves(const Board& board, Position from, std::vector<Move>& out) {
    Side s = board[from.rank][from.file]->side;
    for (int df = -1; df <= 1; ++df)
        for (int dr = -1; dr <= 1; ++dr) {
            if (df == 0 && dr == 0)
                continue;
            Position to{from.file + df, from.rank + dr};
            if (!InBounds(to))
                continue;
            if (!board[to.rank][to.file] || board[to.rank][to.file]->side != s)
                out.push_back(Move{from, to, std::nullopt, false});
        }
}

inline void AddSlidingMoves(const Board& board, Position from, const int* ddf, const int* ddr,
                             std::vector<Move>& out) {
    Side s = board[from.rank][from.file]->side;
    for (int i = 0; i < 4; ++i) {
        Position to{from.file + ddf[i], from.rank + ddr[i]};
        while (InBounds(to)) {
            if (!board[to.rank][to.file]) {
                out.push_back(Move{from, to, std::nullopt, false});
            } else {
                if (board[to.rank][to.file]->side != s)
                    out.push_back(Move{from, to, std::nullopt, false});
                break;
            }
            to.file += ddf[i];
            to.rank += ddr[i];
        }
    }
}

inline void AddCastlingMoves(const Board& board, const GameState& state, Side s,
                              std::vector<Move>& out) {
    int home_rank = (s == Side::White) ? 0 : 7;
    Position king_sq{4, home_rank};
    if (!board[home_rank][4] || board[home_rank][4]->type != PieceType::King ||
        board[home_rank][4]->side != s)
        return;
    Side opp = Opposite(s);
    bool kingside = (s == Side::White) ? state.castling.white_kingside : state.castling.black_kingside;
    bool queenside = (s == Side::White) ? state.castling.white_queenside : state.castling.black_queenside;

    if (kingside && board[home_rank][7] && board[home_rank][7]->type == PieceType::Rook &&
        board[home_rank][7]->side == s) {
        bool path_empty = !board[home_rank][5] && !board[home_rank][6];
        bool safe = !IsSquareAttacked(board, {4, home_rank}, opp) &&
                    !IsSquareAttacked(board, {5, home_rank}, opp) &&
                    !IsSquareAttacked(board, {6, home_rank}, opp);
        if (path_empty && safe)
            out.push_back(Move{king_sq, Position{6, home_rank}, std::nullopt, false});
    }
    if (queenside && board[home_rank][0] && board[home_rank][0]->type == PieceType::Rook &&
        board[home_rank][0]->side == s) {
        // Rook's transit square (file 1 / "b") only needs to be empty, not unattacked.
        bool path_empty = !board[home_rank][1] && !board[home_rank][2] && !board[home_rank][3];
        bool safe = !IsSquareAttacked(board, {4, home_rank}, opp) &&
                    !IsSquareAttacked(board, {3, home_rank}, opp) &&
                    !IsSquareAttacked(board, {2, home_rank}, opp);
        if (path_empty && safe)
            out.push_back(Move{king_sq, Position{2, home_rank}, std::nullopt, false});
    }
}

}  // namespace detail

inline std::vector<Move> GeneratePseudoLegalMoves(const Board& board, const GameState& state) {
    static const int rdf[4] = {1, -1, 0, 0}, rdr[4] = {0, 0, 1, -1};
    static const int bdf[4] = {1, 1, -1, -1}, bdr[4] = {1, -1, 1, -1};

    std::vector<Move> moves;
    Side s = state.side_to_move;
    for (int r = 0; r < 8; ++r) {
        for (int f = 0; f < 8; ++f) {
            if (!board[r][f] || board[r][f]->side != s)
                continue;
            Position from{f, r};
            switch (board[r][f]->type) {
                case PieceType::Pawn:
                    detail::AddPawnMoves(board, state, from, moves);
                    break;
                case PieceType::Knight:
                    detail::AddKnightMoves(board, from, moves);
                    break;
                case PieceType::Bishop:
                    detail::AddSlidingMoves(board, from, bdf, bdr, moves);
                    break;
                case PieceType::Rook:
                    detail::AddSlidingMoves(board, from, rdf, rdr, moves);
                    break;
                case PieceType::Queen:
                    detail::AddSlidingMoves(board, from, rdf, rdr, moves);
                    detail::AddSlidingMoves(board, from, bdf, bdr, moves);
                    break;
                case PieceType::King:
                    detail::AddKingStepMoves(board, from, moves);
                    break;
            }
        }
    }
    detail::AddCastlingMoves(board, state, s, moves);
    return moves;
}

inline bool EnPassantIsCapturable(const Board& board, const GameState& state) {
    if (!state.en_passant_target)
        return false;
    Position t = *state.en_passant_target;
    int capturer_rank = (state.side_to_move == Side::White) ? t.rank - 1 : t.rank + 1;
    for (int df : {-1, 1}) {
        Position p{t.file + df, capturer_rank};
        if (InBounds(p) && board[p.rank][p.file] && board[p.rank][p.file]->side == state.side_to_move &&
            board[p.rank][p.file]->type == PieceType::Pawn)
            return true;
    }
    return false;
}

inline std::string PositionKey(const Board& board, const GameState& state) {
    static const char kSym[] = "PNBRQK";
    std::string key;
    key.reserve(70);
    for (int r = 0; r < 8; ++r)
        for (int f = 0; f < 8; ++f) {
            if (!board[r][f]) {
                key += '.';
                continue;
            }
            char c = kSym[static_cast<int>(board[r][f]->type)];
            key += (board[r][f]->side == Side::White) ? c : static_cast<char>(std::tolower(c));
        }
    key += (state.side_to_move == Side::White) ? 'w' : 'b';
    key += state.castling.white_kingside ? 'K' : '-';
    key += state.castling.white_queenside ? 'Q' : '-';
    key += state.castling.black_kingside ? 'k' : '-';
    key += state.castling.black_queenside ? 'q' : '-';
    key += EnPassantIsCapturable(board, state) ? static_cast<char>('a' + state.en_passant_target->file) : '-';
    return key;
}

inline void ApplyMove(Board& board, GameState& state, const Move& m) {
    Piece moving = *board[m.from.rank][m.from.file];
    std::optional<Piece> captured_normally = board[m.to.rank][m.to.file];
    bool is_capture = captured_normally.has_value() || m.is_en_passant;

    // 1. Halfmove clock.
    state.halfmove_clock = (moving.type == PieceType::Pawn || is_capture) ? 0 : state.halfmove_clock + 1;

    // 2. En passant capture: remove the actual captured pawn (not at `to`).
    if (m.is_en_passant)
        board[m.from.rank][m.to.file] = std::nullopt;

    // 3. Castling: relocate the rook.
    if (moving.type == PieceType::King && std::abs(m.to.file - m.from.file) == 2) {
        int rank = m.from.rank;
        if (m.to.file == 6) {
            board[rank][5] = board[rank][7];
            board[rank][7] = std::nullopt;
        } else if (m.to.file == 2) {
            board[rank][3] = board[rank][0];
            board[rank][0] = std::nullopt;
        }
    }

    // 4. Castling rights updates.
    if (moving.type == PieceType::King) {
        if (moving.side == Side::White) {
            state.castling.white_kingside = false;
            state.castling.white_queenside = false;
        } else {
            state.castling.black_kingside = false;
            state.castling.black_queenside = false;
        }
    }
    if (moving.type == PieceType::Rook) {
        if (m.from == Position{0, 0}) state.castling.white_queenside = false;
        if (m.from == Position{7, 0}) state.castling.white_kingside = false;
        if (m.from == Position{0, 7}) state.castling.black_queenside = false;
        if (m.from == Position{7, 7}) state.castling.black_kingside = false;
    }
    // Capturing an unmoved enemy rook on its home square also revokes that right.
    if (m.to == Position{0, 0}) state.castling.white_queenside = false;
    if (m.to == Position{7, 0}) state.castling.white_kingside = false;
    if (m.to == Position{0, 7}) state.castling.black_queenside = false;
    if (m.to == Position{7, 7}) state.castling.black_kingside = false;

    // 5. Place moving piece (substituting promotion type if any), clear origin.
    board[m.to.rank][m.to.file] = m.promotion ? Piece{*m.promotion, moving.side} : moving;
    board[m.from.rank][m.from.file] = std::nullopt;

    // 6. En passant target.
    state.en_passant_target = std::nullopt;
    if (moving.type == PieceType::Pawn && std::abs(m.to.rank - m.from.rank) == 2)
        state.en_passant_target = Position{m.from.file, (m.from.rank + m.to.rank) / 2};

    // 7. Fullmove number increments after Black moves.
    if (moving.side == Side::Black)
        ++state.fullmove_number;

    // 8. Side to move flips.
    state.side_to_move = Opposite(state.side_to_move);

    // 9. Bookkeeping (after all mutations above).
    state.moves_uci.push_back(UciMoveString(m));
    state.position_history[PositionKey(board, state)]++;
}

inline std::vector<Move> GenerateLegalMoves(const Board& board, const GameState& state) {
    std::vector<Move> legal;
    Side mover = state.side_to_move;
    for (const Move& m : GeneratePseudoLegalMoves(board, state)) {
        Board b2 = board;
        GameState s2 = state;
        ApplyMove(b2, s2, m);
        Position king = FindKing(b2, mover);
        if (!IsSquareAttacked(b2, king, Opposite(mover)))
            legal.push_back(m);
    }
    return legal;
}

inline bool HasInsufficientMaterial(const Board& board) {
    int minors = 0;
    std::vector<Position> bishops;
    for (int r = 0; r < 8; ++r)
        for (int f = 0; f < 8; ++f) {
            if (!board[r][f])
                continue;
            switch (board[r][f]->type) {
                case PieceType::Pawn:
                case PieceType::Rook:
                case PieceType::Queen:
                    return false;
                case PieceType::Knight:
                    ++minors;
                    break;
                case PieceType::Bishop:
                    ++minors;
                    bishops.push_back({f, r});
                    break;
                default:
                    break;  // King
            }
        }
    if (minors == 0) return true;   // K vs K
    if (minors == 1) return true;   // K+minor vs K
    if (minors == 2 && bishops.size() == 2)
        return IsDarkSquare(bishops[0]) == IsDarkSquare(bishops[1]);
    return false;
}

enum class GameResult { Ongoing, Checkmate, Stalemate, DrawFiftyMove, DrawThreefold, DrawInsufficientMaterial };

inline GameResult ComputeGameResult(const Board& board, const GameState& state,
                                     const std::vector<Move>& legal_moves_for_side_to_move) {
    if (legal_moves_for_side_to_move.empty()) {
        Position king = FindKing(board, state.side_to_move);
        return IsSquareAttacked(board, king, Opposite(state.side_to_move)) ? GameResult::Checkmate
                                                                            : GameResult::Stalemate;
    }
    if (state.halfmove_clock >= 100)
        return GameResult::DrawFiftyMove;
    auto it = state.position_history.find(PositionKey(board, state));
    if (it != state.position_history.end() && it->second >= 3)
        return GameResult::DrawThreefold;
    if (HasInsufficientMaterial(board))
        return GameResult::DrawInsufficientMaterial;
    return GameResult::Ongoing;
}
