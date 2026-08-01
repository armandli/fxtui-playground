#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <signal.h>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "board.h"
#include "common/child_process.h"
#include "common/grid_render.h"
#include "common/mouse_input.h"
#include "move_gen.h"
#include "piece.h"
#include "piece_bitmaps.h"
#include "stockfish_client.h"

using namespace common;
using namespace ftxui;

namespace {

void PrintUsage(std::ostream& out, const char* prog) {
    out << "usage: " << prog << " [--elo N | --movetime MS | --depth D] [-h]\n"
        << "  --elo N        limit Stockfish's strength via UCI_Elo (1320-3190)\n"
        << "  --movetime MS  full-strength Stockfish, fixed think time (ms) per move\n"
        << "  --depth D      full-strength Stockfish, fixed search depth per move\n"
        << "  (no flags)     equivalent to --elo 1500\n";
}

// Returns an exit code if the program should stop now (bad args, -h/--help),
// or nullopt to continue with `config` populated.
std::optional<int> ParseArgs(int argc, char** argv, StockfishClient::DifficultyConfig& config) {
    bool mode_set = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next_int = [&](int& out) -> bool {
            if (i + 1 >= argc)
                return false;
            try {
                size_t pos = 0;
                out = std::stoi(argv[++i], &pos);
                return pos == std::string(argv[i]).size();
            } catch (...) {
                return false;
            }
        };
        if (arg == "-h" || arg == "--help") {
            PrintUsage(std::cout, argv[0]);
            return 0;
        }
        if (mode_set) {
            PrintUsage(std::cerr, argv[0]);
            return 1;
        }
        if (arg == "--elo") {
            int v = 0;
            if (!next_int(v) || v < 1320 || v > 3190) {
                PrintUsage(std::cerr, argv[0]);
                return 1;
            }
            config.limit_strength = true;
            config.elo = v;
            config.go_mode = StockfishClient::GoMode::Movetime;
            config.go_value = 1000;
            mode_set = true;
        } else if (arg == "--movetime") {
            int v = 0;
            if (!next_int(v) || v <= 0) {
                PrintUsage(std::cerr, argv[0]);
                return 1;
            }
            config.limit_strength = false;
            config.go_mode = StockfishClient::GoMode::Movetime;
            config.go_value = v;
            mode_set = true;
        } else if (arg == "--depth") {
            int v = 0;
            if (!next_int(v) || v <= 0) {
                PrintUsage(std::cerr, argv[0]);
                return 1;
            }
            config.limit_strength = false;
            config.go_mode = StockfishClient::GoMode::Depth;
            config.go_value = v;
            mode_set = true;
        } else {
            PrintUsage(std::cerr, argv[0]);
            return 1;
        }
    }
    return std::nullopt;
}

enum class GamePhase { ChoosingColor, Playing, ChoosingPromotion, GameOver };

constexpr int kSidePanelWidth = 32;

// Below this, a piece's pixel canvas (2*scale x 2*scale) is too coarse to
// read as a bitmap silhouette, so RenderCell falls back to the plain glyph.
constexpr int kMinBitmapScale = 2;

// Default board coloring per the Wikipedia chess page convention (a1 dark, h1
// light), using the widely-recognized "Wikipedia-style" palette.
const Color kLightSquare = Color::RGB(255, 206, 158);
const Color kDarkSquare = Color::RGB(209, 139, 71);
const Color kSelectedHighlight = Color::RGB(246, 235, 100);
const Color kCheckHighlight = Color::RGB(210, 70, 70);
const Color kWhitePieceFg = Color::RGB(255, 255, 255);
const Color kBlackPieceFg = Color::RGB(15, 15, 15);

// Orientation: the human's own pieces always render at the bottom of the
// screen. Self-inverse: applying the same formula converts a display
// row/col back to a board rank/file.
int DisplayRow(int rank, Side human) { return human == Side::White ? 7 - rank : rank; }
int DisplayCol(int file, Side human) { return human == Side::White ? file : 7 - file; }

Element RenderCell(const Board& board, Position pos, int scale, bool is_selected, bool is_legal_target,
                    bool is_check) {
    Color bg = IsDarkSquare(pos) ? kDarkSquare : kLightSquare;
    if (is_check)
        bg = kCheckHighlight;
    else if (is_selected)
        bg = kSelectedHighlight;
    else if (is_legal_target)
        bg = Color::Interpolate(0.4f, bg, Color::RGB(40, 130, 60));

    if (!board[pos.rank][pos.file])
        return RenderGlyphCell(bg, Color::White, "", scale);

    const Piece& p = *board[pos.rank][pos.file];
    Color fg = (p.side == Side::White) ? kWhitePieceFg : kBlackPieceFg;
    if (scale < kMinBitmapScale)
        return RenderGlyphCell(bg, fg, PieceGlyph(p.type), scale);
    return RenderPieceCell(bg, fg, PieceBitmapFor(p.type), scale);
}

Element RenderBoard(const Board& board, int scale, Side human_color, const std::optional<Position>& selected,
                     const std::vector<Move>& legal, const std::optional<Position>& king_in_check) {
    std::vector<Position> targets;
    if (selected) {
        for (auto& m : legal)
            if (m.from == *selected)
                targets.push_back(m.to);
    }
    return RenderGrid(8, 8, [&](int display_row, int display_col) {
        // DisplayCol/DisplayRow are self-inverse, so calling them on a display
        // coordinate recovers the underlying board file/rank.
        Position pos{DisplayCol(display_col, human_color), DisplayRow(display_row, human_color)};
        bool is_selected = selected.has_value() && *selected == pos;
        bool is_target = std::find(targets.begin(), targets.end(), pos) != targets.end();
        bool is_check = king_in_check.has_value() && *king_in_check == pos;
        return RenderCell(board, pos, scale, is_selected, is_target, is_check);
    });
}

int ComputeScale(Dimensions term) {
    int avail_rows = std::max(1, term.dimy - 2);                    // board border top/bottom
    int avail_cols = std::max(1, term.dimx - kSidePanelWidth - 3);  // border + separator + side panel
    return ComputeGridScale(avail_rows, avail_cols, 8, 8);
}

std::optional<Position> KingInCheckSquare(const Board& board, const GameState& state) {
    Position king = FindKing(board, state.side_to_move);
    if (IsSquareAttacked(board, king, Opposite(state.side_to_move)))
        return king;
    return std::nullopt;
}

Element RenderPromoChoice(PieceType t, Side side) {
    Color fg = (side == Side::White) ? kWhitePieceFg : kBlackPieceFg;
    return text(" " + PieceGlyph(t) + " ") | color(fg) | bgcolor(Color::RGB(90, 90, 90)) | border;
}

std::string DifficultyText(const StockfishClient::DifficultyConfig& c) {
    if (c.limit_strength)
        return "Difficulty: ~" + std::to_string(c.elo) + " Elo";
    if (c.go_mode == StockfishClient::GoMode::Movetime)
        return "Difficulty: full strength, " + std::to_string(c.go_value) + "ms/move";
    return "Difficulty: full strength, depth " + std::to_string(c.go_value);
}

std::string ResultText(GameResult r, Side side_to_move) {
    switch (r) {
        case GameResult::Checkmate:
            return std::string(side_to_move == Side::White ? "Black" : "White") + " wins by checkmate";
        case GameResult::Stalemate:
            return "Draw by stalemate";
        case GameResult::DrawFiftyMove:
            return "Draw by fifty-move rule";
        case GameResult::DrawThreefold:
            return "Draw by threefold repetition";
        case GameResult::DrawInsufficientMaterial:
            return "Draw by insufficient material";
        default:
            return "";
    }
}

}  // namespace

int main(int argc, char** argv) {
    // common::TerminateChildAndReraise is load-bearing only before
    // screen.Loop() starts (CLI parsing + the blocking Start() handshake) and
    // briefly after it returns during our own cleanup. While the TUI is
    // actually running, FTXUI's own ScreenInteractive::Loop() installs
    // handlers for SIGINT/SIGTERM (and others) that convert the signal into
    // a graceful Loop() return, restoring this handler once Loop() exits --
    // so ~StockfishClient() already runs cleanly via normal scope-unwind
    // during gameplay, no special handling needed there.
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, TerminateChildAndReraise);
    signal(SIGTERM, TerminateChildAndReraise);

    StockfishClient::DifficultyConfig config;
    if (auto code = ParseArgs(argc, argv, config))
        return *code;

    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    StockfishClient client("stockfish", [&] { screen.PostEvent(Event::Custom); });
    if (!client.Start(config)) {
        std::cerr << "chess: failed to start or communicate with 'stockfish' "
                     "(is it installed and on PATH?)\n";
        return 1;
    }

    GamePhase phase = GamePhase::ChoosingColor;
    Board board = InitialBoard();
    GameState state;
    state.position_history[PositionKey(board, state)] = 1;
    std::vector<Move> legal = GenerateLegalMoves(board, state);
    Side human_color = Side::White;
    std::optional<Position> selected;
    Position pending_from, pending_to;
    GameResult result = GameResult::Ongoing;
    std::string game_over_text;

    Box board_box;
    Box white_choice_box, black_choice_box;
    Box promo_q_box, promo_r_box, promo_b_box, promo_n_box;

    auto ApplyMoveAndAdvance = [&](const Move& m) {
        ApplyMove(board, state, m);
        legal = GenerateLegalMoves(board, state);
        result = ComputeGameResult(board, state, legal);
        selected.reset();
        if (result != GameResult::Ongoing) {
            phase = GamePhase::GameOver;
            game_over_text = ResultText(result, state.side_to_move);
        } else {
            phase = GamePhase::Playing;
            if (state.side_to_move != human_color)
                client.RequestBestMove(state.moves_uci);
        }
    };

    auto ResetGame = [&] {
        board = InitialBoard();
        state = GameState{};
        state.position_history[PositionKey(board, state)] = 1;
        legal = GenerateLegalMoves(board, state);
        selected.reset();
        result = GameResult::Ongoing;
        game_over_text.clear();
    };

    auto StartGame = [&](Side side) {
        human_color = side;
        ResetGame();
        phase = GamePhase::Playing;
        if (human_color == Side::Black)
            client.RequestBestMove({});
    };

    auto renderer = Renderer([&] {
        Element board_or_prompt;
        Element side_panel;
        auto term = Terminal::Size();

        if (phase == GamePhase::ChoosingColor) {
            board_or_prompt = vbox({
                                   text("Chess vs Stockfish") | bold | center,
                                   text(""),
                                   text("Choose your side:") | center,
                                   text(""),
                                   hbox({
                                       text("  Play White  ") | bgcolor(Color::RGB(240, 240, 240)) |
                                           color(Color::Black) | border | reflect(white_choice_box),
                                       text("   "),
                                       text("  Play Black  ") | bgcolor(Color::RGB(30, 30, 30)) |
                                           color(Color::White) | border | reflect(black_choice_box),
                                   }) | center,
                               }) |
                               center | flex;
            side_panel = text("") | size(WIDTH, EQUAL, kSidePanelWidth);
        } else {
            int scale = ComputeScale(term);
            auto king_check = KingInCheckSquare(board, state);
            Element backdrop =
                RenderBoard(board, scale, human_color, selected, legal, king_check) | reflect(board_box) | border;

            if (phase == GamePhase::ChoosingPromotion) {
                Element prompt = vbox({
                                      text("Promote pawn to:") | bold | center,
                                      hbox({
                                          RenderPromoChoice(PieceType::Queen, human_color) | reflect(promo_q_box),
                                          RenderPromoChoice(PieceType::Rook, human_color) | reflect(promo_r_box),
                                          RenderPromoChoice(PieceType::Bishop, human_color) | reflect(promo_b_box),
                                          RenderPromoChoice(PieceType::Knight, human_color) | reflect(promo_n_box),
                                      }),
                                  }) |
                                  border | bgcolor(Color::RGB(30, 30, 30)) | center;
                board_or_prompt = dbox({backdrop, prompt});
            } else {
                board_or_prompt = backdrop;
            }

            Elements panel_lines;
            panel_lines.push_back(text("Chess vs Stockfish") | bold);
            panel_lines.push_back(text(DifficultyText(config)));
            panel_lines.push_back(text(std::string("You are: ") + (human_color == Side::White ? "White" : "Black")));
            panel_lines.push_back(separator());
            if (phase == GamePhase::GameOver) {
                panel_lines.push_back(text(game_over_text) | bold);
            } else if (client.IsThinking()) {
                panel_lines.push_back(text("Stockfish is thinking...") | bold);
            } else {
                panel_lines.push_back(text(state.side_to_move == human_color ? "Your move" : "Waiting for engine...") |
                                       bold);
            }
            panel_lines.push_back(separator());
            panel_lines.push_back(text("Moves:"));
            size_t total_pairs = (state.moves_uci.size() + 1) / 2;
            size_t start_pair = (total_pairs > 15) ? total_pairs - 15 : 0;
            if (start_pair > 0)
                panel_lines.push_back(text("  ..."));
            for (size_t i = start_pair * 2; i < state.moves_uci.size(); i += 2) {
                std::string line = "  " + std::to_string(i / 2 + 1) + ". " + state.moves_uci[i];
                if (i + 1 < state.moves_uci.size())
                    line += "  " + state.moves_uci[i + 1];
                panel_lines.push_back(text(line));
            }
            panel_lines.push_back(filler());
            panel_lines.push_back(separator());
            panel_lines.push_back(text("click piece to select, click"));
            panel_lines.push_back(text("highlighted square to move"));
            panel_lines.push_back(text("n = new game   q = quit"));
            side_panel = vbox(std::move(panel_lines)) | size(WIDTH, EQUAL, kSidePanelWidth);
        }

        return hbox({board_or_prompt, separator(), side_panel});
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            if (phase == GamePhase::Playing) {
                if (auto uci = client.TakeBestMove()) {
                    auto it = std::find_if(legal.begin(), legal.end(),
                                            [&](const Move& m) { return UciMoveString(m) == *uci; });
                    if (it == legal.end()) {
                        phase = GamePhase::GameOver;
                        game_over_text = "Engine returned unrecognized move: " + *uci;
                    } else {
                        ApplyMoveAndAdvance(*it);
                    }
                }
            }
            return false;  // let the renderer run after the state update
        }

        if (e == Event::Character('q') || e == Event::Character('Q') || e == Event::Escape) {
            screen.Exit();
            return true;
        }
        if (e == Event::Character('n') || e == Event::Character('N')) {
            client.CancelPending();
            client.NewGame();
            phase = GamePhase::ChoosingColor;
            return true;
        }

        if (!IsLeftClickPress(e))
            return false;
        int mx = e.mouse().x, my = e.mouse().y;

        if (phase == GamePhase::ChoosingColor) {
            if (white_choice_box.Contain(mx, my)) {
                StartGame(Side::White);
                return true;
            }
            if (black_choice_box.Contain(mx, my)) {
                StartGame(Side::Black);
                return true;
            }
            return false;
        }

        if (phase == GamePhase::ChoosingPromotion) {
            std::optional<PieceType> chosen;
            if (promo_q_box.Contain(mx, my)) chosen = PieceType::Queen;
            else if (promo_r_box.Contain(mx, my)) chosen = PieceType::Rook;
            else if (promo_b_box.Contain(mx, my)) chosen = PieceType::Bishop;
            else if (promo_n_box.Contain(mx, my)) chosen = PieceType::Knight;
            if (!chosen)
                return false;
            auto it = std::find_if(legal.begin(), legal.end(), [&](const Move& m) {
                return m.from == pending_from && m.to == pending_to && m.promotion == chosen;
            });
            if (it != legal.end())
                ApplyMoveAndAdvance(*it);
            return true;
        }

        if (phase != GamePhase::Playing || state.side_to_move != human_color)
            return false;
        if (!board_box.Contain(mx, my))
            return false;

        // Note: board_box's own x_max/y_max cannot be used to derive cell size --
        // hbox stretches every child to a uniform cross-axis height (driven here
        // by the side panel's filler()), so board_box.y_max reports the full
        // stretched height, not the board's tight content height. Use the same
        // ComputeScale() the renderer used instead, anchored at board_box's origin.
        int scale = ComputeScale(Terminal::Size());
        auto [display_row, display_col] = PixelToCell(mx, my, board_box, scale);
        if (display_col < 0 || display_col >= 8 || display_row < 0 || display_row >= 8)
            return false;
        Position clicked{DisplayCol(display_col, human_color), DisplayRow(display_row, human_color)};

        if (!selected) {
            if (board[clicked.rank][clicked.file] && board[clicked.rank][clicked.file]->side == human_color)
                selected = clicked;
        } else if (clicked == *selected) {
            selected.reset();
        } else if (board[clicked.rank][clicked.file] && board[clicked.rank][clicked.file]->side == human_color) {
            selected = clicked;
        } else {
            std::vector<Move> matches;
            for (auto& m : legal)
                if (m.from == *selected && m.to == clicked)
                    matches.push_back(m);
            if (matches.empty()) {
                selected.reset();
            } else if (matches.size() == 1) {
                ApplyMoveAndAdvance(matches[0]);
            } else {
                phase = GamePhase::ChoosingPromotion;
                pending_from = *selected;
                pending_to = clicked;
                selected.reset();
            }
        }
        return true;
    });

    screen.Loop(app);
    return 0;  // ~StockfishClient() runs here via normal stack unwind
}
