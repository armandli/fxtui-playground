#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

// ─── Character density groups ────────────────────────────────────────────────
//
// Characters are grouped 0 (fewest pixels) → 4 (most pixels).
// A raindrop is rendered top-to-bottom with density increasing toward the head
// (bottom), giving the teardrop silhouette the user requested.
//
// All characters chosen are single terminal-column wide to keep the grid
// aligned.  Half-width Katakana (U+FF65–U+FF9F) are used for groups 2–3
// because they are the classic Matrix glyphs and are always 1-column wide.
//
// Pixel-density rationale per group:
//   0 – punctuation / combining dots  : barely any ink
//   1 – light-shade block + thin marks: a few horizontal pixels
//   2 – half-width Katakana body      : typical glyph density
//   3 – ASCII heavyweights + Katakana : wide, multi-stroke glyphs
//   4 – solid/dark-shade block chars  : maximum fill

namespace {

const std::array<std::vector<std::string>, 5> kGroups = {{
    // 0: ghost / trailing wisps
    {".", "\xC2\xB7"/*·*/, "'", "`", ",", ";", ":", "!"},
    // 1: faint — light shade + thin marks
    {"\xE2\x96\x91"/*░*/, "-", "=", "+", "\xC2\xB0"/*°*/,
     "\xEF\xBD\xB0"/*ｰ*/, "|", "~"},
    // 2: medium — half-width Katakana (classic Matrix glyphs)
    {"\xEF\xBD\xB1"/*ｱ*/, "\xEF\xBD\xB2"/*ｲ*/, "\xEF\xBD\xB3"/*ｳ*/,
     "\xEF\xBD\xB4"/*ｴ*/, "\xEF\xBD\xB5"/*ｵ*/, "\xEF\xBD\xB6"/*ｶ*/,
     "\xEF\xBD\xB7"/*ｷ*/, "\xEF\xBD\xB8"/*ｸ*/, "\xEF\xBD\xB9"/*ｹ*/,
     "\xEF\xBD\xBA"/*ｺ*/, "\xEF\xBD\xBB"/*ｻ*/, "\xEF\xBD\xBC"/*ｼ*/,
     "\xEF\xBD\xBD"/*ｽ*/, "\xEF\xBD\xBE"/*ｾ*/, "\xEF\xBD\xBF"/*ｿ*/,
     "\xEF\xBE\x80"/*ﾀ*/, "\xEF\xBE\x81"/*ﾁ*/, "\xEF\xBE\x82"/*ﾂ*/,
     "\xEF\xBE\x83"/*ﾃ*/, "\xEF\xBE\x84"/*ﾄ*/, "\xEF\xBE\x85"/*ﾅ*/,
     "\xEF\xBE\x86"/*ﾆ*/, "\xEF\xBE\x87"/*ﾇ*/, "\xEF\xBE\x88"/*ﾈ*/,
     "\xEF\xBE\x89"/*ﾉ*/, "\xEF\xBE\x8A"/*ﾊ*/, "\xEF\xBE\x8B"/*ﾋ*/,
     "\xEF\xBE\x8C"/*ﾌ*/, "\xEF\xBE\x8D"/*ﾍ*/, "\xEF\xBE\x8E"/*ﾎ*/,
     "\xEF\xBE\x8F"/*ﾏ*/, "\xEF\xBE\x90"/*ﾐ*/, "\xEF\xBE\x91"/*ﾑ*/,
     "\xEF\xBE\x92"/*ﾒ*/, "\xEF\xBE\x93"/*ﾓ*/, "\xEF\xBE\x94"/*ﾔ*/,
     "\xEF\xBE\x95"/*ﾕ*/, "\xEF\xBE\x96"/*ﾖ*/, "\xEF\xBE\x97"/*ﾗ*/,
     "\xEF\xBE\x98"/*ﾘ*/, "\xEF\xBE\x99"/*ﾙ*/, "\xEF\xBE\x9A"/*ﾚ*/,
     "\xEF\xBE\x9B"/*ﾛ*/, "\xEF\xBE\x9C"/*ﾜ*/, "\xEF\xBE\x9D"/*ﾝ*/},
    // 3: dense — heavy ASCII + few dense Katakana
    {"\xE2\x96\x92"/*▒*/, "#", "@", "W", "M", "&", "%", "$",
     "\xEF\xBE\x9C"/*ﾜ*/, "\xEF\xBE\x9D"/*ﾝ*/},
    // 4: head / maximum fill
    {"\xE2\x96\x93"/*▓*/, "\xE2\x96\x88"/*█*/},
}};

std::mt19937 g_rng{std::random_device{}()};

// Returns a random character from the density group appropriate for position
// `pos` within a drop of length `len` (pos 0 = tail/top, pos len-1 = head/bottom).
const std::string& pick_char(int pos, int len) {
    int denom = std::max(len - 1, 1);
    int group  = std::clamp((pos * 4) / denom, 0, 4);
    const auto& g = kGroups[group];
    std::uniform_int_distribution<int> d(0, static_cast<int>(g.size()) - 1);
    return g[d(g_rng)];
}

// Green color shading: tail (pos=0) is near-black, head (pos=len-1) is bright.
// Quadratic ramp gives a sharper brightness peak near the head.
Color drop_color(int pos, int len) {
    if (pos == len - 1)
        return Color::RGB(200, 255, 200);  // brilliant head
    float t = static_cast<float>(pos) / static_cast<float>(std::max(len - 1, 1));
    int g = static_cast<int>(t * t * 220.f) + 10;
    return Color::RGB(0, g, 0);
}

// ─── State ───────────────────────────────────────────────────────────────────

struct Drop {
    int col;
    int head;  // row of the leading (bottom) character; may be negative while entering
    int len;   // 6–8
};

struct Cell {
    std::string ch    = " ";
    Color       fg    = Color::Default;
    bool        active = false;
};

using Grid = std::vector<std::vector<Cell>>;

Grid make_grid(int w, int h) {
    return Grid(h, std::vector<Cell>(w));
}

}  // namespace

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    auto sz = Terminal::Size();
    int W = sz.dimx;
    int H = sz.dimy;

    // Double buffers: front is what the renderer reads; back is what update writes.
    Grid front = make_grid(W, H);
    Grid back  = make_grid(W, H);

    std::vector<Drop> drops;
    drops.reserve(4096);

    std::uniform_int_distribution<int>  len_rng(6, 8);
    std::uniform_real_distribution<float> prob_rng(0.f, 1.f);

    constexpr float kSpawnProb = 0.30f;

    // Called once per 100 ms tick on the main thread via Event::Custom.
    auto update = [&] {
        // Resize buffers if terminal changed.
        auto s = Terminal::Size();
        if (s.dimx != W || s.dimy != H) {
            W = s.dimx; H = s.dimy;
            front = make_grid(W, H);
            back  = make_grid(W, H);
            drops.clear();
        }

        // Move every drop one row down.
        for (auto& d : drops) d.head++;

        // Retire drops that have fully left the bottom of the screen.
        drops.erase(
            std::remove_if(drops.begin(), drops.end(),
                [&](const Drop& d) { return d.head - d.len >= H; }),
            drops.end());

        // Randomly spawn new drops at the top.
        for (int c = 0; c < W; ++c) {
            if (prob_rng(g_rng) < kSpawnProb) {
                int len = len_rng(g_rng);
                drops.push_back({c, -len, len});
            }
        }

        // Clear the write (back) buffer.
        for (auto& row : back)
            for (auto& cell : row) { cell.active = false; cell.ch = " "; }

        // Paint each drop into the back buffer.
        // pos 0 = top of drop (sparse/dark), pos len-1 = head/bottom (dense/bright).
        for (const auto& d : drops) {
            for (int i = 0; i < d.len; ++i) {
                int row = d.head - (d.len - 1 - i);
                int col = d.col;
                if (row >= 0 && row < H && col >= 0 && col < W) {
                    back[row][col] = {pick_char(i, d.len), drop_color(i, d.len), true};
                }
            }
        }

        // Swap buffers: the renderer will read the newly painted front next frame.
        std::swap(front, back);
    };

    // Renderer reads only from front (written by update, never concurrently).
    // Consecutive spaces in a row are collapsed into a single text() to reduce
    // the element count — a 30 % active-cell density drops from ~width elements
    // per row to roughly 2 × active_cells_per_row.
    auto renderer = Renderer([&] {
        Elements rows;
        rows.reserve(H);

        for (int r = 0; r < H; ++r) {
            Elements elems;
            std::string space_run;

            for (int c = 0; c < W; ++c) {
                const auto& cell = front[r][c];
                if (!cell.active) {
                    space_run += ' ';
                } else {
                    if (!space_run.empty()) {
                        elems.push_back(text(space_run));
                        space_run.clear();
                    }
                    elems.push_back(text(cell.ch) | color(cell.fg));
                }
            }
            if (!space_run.empty())
                elems.push_back(text(space_run));

            rows.push_back(hbox(std::move(elems)));
        }

        return vbox(std::move(rows));
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            update();
            return false;  // let the renderer run after state update
        }
        if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    // Timer thread: fires every 100 ms and posts a Custom event to the main loop.
    std::atomic<bool> running{true};
    std::thread timer([&] {
        while (running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (running.load(std::memory_order_relaxed))
                screen.PostEvent(Event::Custom);
        }
    });

    screen.Loop(app);

    running.store(false, std::memory_order_relaxed);
    timer.join();
}
