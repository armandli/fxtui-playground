#ifndef PIECE_BITMAPS_H
#define PIECE_BITMAPS_H

#include <cassert>
#include <cstdint>

#include <algorithm>
#include <array>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <piece.h>

// 16x16 monochrome silhouette bitmaps for each chess piece type, rendered
// via the upper-half-block technique (see render_piece_cell) so a piece
// scales to fill its board cell instead of staying a single fixed-size
// glyph. Side (white/black) is conveyed purely by foreground color, same as
// piece_glyph in piece.h -- these bitmaps are shape-only, not per-side.
//
// Each row is a 16-bit mask; bit (kPieceBitmapSize-1-x) is column x, i.e.
// the most significant bit is the leftmost pixel.

constexpr int kPieceBitmapSize = 16;
using PieceBitmap = std::array<uint16_t, kPieceBitmapSize>;

inline constexpr PieceBitmap kPawnBitmap = {
    0x0000, 0x0180, 0x03C0, 0x07E0, 0x07E0, 0x03C0, 0x0180, 0x0180,
    0x03C0, 0x07E0, 0x0FF0, 0x0FF0, 0x1FF8, 0x3FFC, 0x3FFC, 0x0000,
};

inline constexpr PieceBitmap kRookBitmap = {
    0x73CE, 0x73CE, 0x7FFE, 0x3FFC, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8,
    0x1FF8, 0x1FF8, 0x1FF8, 0x3FFC, 0x3FFC, 0x7FFE, 0x7FFE, 0x0000,
};

inline constexpr PieceBitmap kBishopBitmap = {
    0x0180, 0x0180, 0x03C0, 0x07E0, 0x0FF0, 0x0E70, 0x0FF0, 0x07E0,
    0x03C0, 0x07E0, 0x0FF0, 0x1FF8, 0x3FFC, 0x3FFC, 0x7FFE, 0x0000,
};

// The only asymmetric bitmap: a horse head silhouette facing left (nose
// reaching the leftmost column around the vertical midpoint).
inline constexpr PieceBitmap kKnightBitmap = {
    0x000F, 0x001F, 0x00FF, 0x03FF, 0x07FF, 0x1FFF, 0x7FFF, 0xFFFE,
    0xFFF0, 0xFFC0, 0x3FC0, 0x0FF0, 0x07F8, 0x1FFC, 0x3FFE, 0x7FFF,
};

inline constexpr PieceBitmap kQueenBitmap = {
    0x6DB6, 0x7FFE, 0x1FF8, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0,
    0x1FF8, 0x1FF8, 0x3FFC, 0x3FFC, 0x7FFE, 0x7FFE, 0xFFFF, 0x0000,
};

inline constexpr PieceBitmap kKingBitmap = {
    0x0180, 0x07E0, 0x0180, 0x07E0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0,
    0x1FF8, 0x1FF8, 0x1FF8, 0x3FFC, 0x3FFC, 0x7FFE, 0x7FFE, 0xFFFF,
};

inline const PieceBitmap& piece_bitmap_for(PieceType t) {
  switch (t) {
    case PieceType::King:
      return kKingBitmap;

    break; case PieceType::Queen:
      return kQueenBitmap;

    break; case PieceType::Rook:
      return kRookBitmap;

    break; case PieceType::Bishop:
      return kBishopBitmap;

    break; case PieceType::Knight:
      return kKnightBitmap;

    break; case PieceType::Pawn:
      return kPawnBitmap;

    break; default:
      assert(false);  // should never get here
      return kPawnBitmap;
  }
}

inline bool piece_bitmap_pixel_on(const PieceBitmap& bitmap, int x, int y) {
  return (bitmap[y] >> (kPieceBitmapSize - 1 - x)) & 1;
}

// Renders a scale-tall, 2*scale-wide cell (matching render_glyph_cell's
// footprint exactly, so compute_grid_scale/pixel_to_cell need no changes) by
// nearest-neighbor sampling `bitmap` through the upper-half-block technique:
// each character encodes 2 vertical pixels (fg = top, bg = bottom), 1 pixel
// per character column -- the same trick party_parrot/main.cpp uses to
// scale GIF frames into the terminal.
inline ftxui::Element render_piece_cell(
    ftxui::Color bg,
    ftxui::Color fg,
    const PieceBitmap& bitmap,
    int scale)
{
  using namespace ftxui;
  Elements rows;
  for (int ty = 0; ty < scale; ++ty) {
    Elements cells;
    for (int tx = 0; tx < 2 * scale; ++tx) {
      int bx = std::clamp(
          tx * kPieceBitmapSize / (2 * scale),
          0,
          kPieceBitmapSize - 1);
      int by_top = std::clamp(
          ty * kPieceBitmapSize / scale,
          0,
          kPieceBitmapSize - 1);
      int by_bot = std::clamp(
          (2 * ty + 1) * kPieceBitmapSize / (2 * scale),
          0,
          kPieceBitmapSize - 1);
      Color top = piece_bitmap_pixel_on(bitmap, bx, by_top) ? fg : bg;
      Color bottom = piece_bitmap_pixel_on(bitmap, bx, by_bot) ? fg : bg;
      cells.push_back(text("\xE2\x96\x80") | color(top) | bgcolor(bottom));
    }
    rows.push_back(hbox(std::move(cells)));
  }
  return vbox(std::move(rows));
}

#endif  // PIECE_BITMAPS_H
