#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <utility>
#include <vector>

using namespace ftxui;

namespace {

constexpr int kRows = 100;
constexpr int kCols = 100;
static_assert(kRows % 2 == 0, "kRows must be even to pair rows for rendering");

constexpr float kInitialDensity = 0.30f;
constexpr auto kTickInterval = std::chrono::seconds(1);

inline int wrap(int v, int n) {
    if (v < 0) return v + n;
    if (v >= n) return v - n;
    return v;
}

inline int idx(int r, int c) { return r * kCols + c; }

Color RandomColor(std::mt19937& rng) {
    std::uniform_int_distribution<int> hue(0, 255);
    return Color::HSV(static_cast<uint8_t>(hue(rng)), 200, 255);
}

// One generation: an alive flag plus a color assigned at birth and kept
// unchanged for as long as the cell stays alive.
struct World {
    std::vector<uint8_t> alive = std::vector<uint8_t>(kRows * kCols, 0);
    std::vector<Color>   color = std::vector<Color>(kRows * kCols, Color::Black);
};

// Splits [0, rows) into one contiguous chunk per hardware thread and runs
// body(row_begin, row_end) for each chunk concurrently, blocking until all
// finish. Used below so a whole generation update is computed by independent
// worker threads instead of a single thread walking every cell in turn.
template <typename F>
void parallel_for_rows(int rows, F&& body) {
    int nthreads = static_cast<int>(std::thread::hardware_concurrency());
    if (nthreads <= 0) nthreads = 4;
    nthreads = std::clamp(nthreads, 1, rows);

    int chunk = (rows + nthreads - 1) / nthreads;
    std::vector<std::thread> workers;
    workers.reserve(nthreads);
    for (int t = 0; t < nthreads; ++t) {
        int begin = t * chunk;
        int end = std::min(rows, begin + chunk);
        if (begin >= end) break;
        workers.emplace_back(body, begin, end);
    }
    for (auto& w : workers) w.join();
}

}  // namespace

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    World front, back;

    // hsum[r*kCols+c] = alive[r][c-1] + alive[r][c] + alive[r][c+1] (wrapped).
    // Recomputed every generation and reused across ticks to avoid reallocating.
    std::vector<uint8_t> hsum(kRows * kCols, 0);

    // Random initial population (toroidal grid, wraps at all edges).
    {
        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> density(0.f, 1.f);
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                if (density(rng) < kInitialDensity) {
                    front.alive[idx(r, c)] = 1;
                    front.color[idx(r, c)] = RandomColor(rng);
                }
            }
        }
    }

    // Advances the simulation by one generation (front -> back, then swap).
    //
    // Neighbor counts are computed in two embarrassingly-parallel passes
    // rather than by summing 8 neighbors per cell one at a time:
    //   1. Horizontal pass: 3-wide row sums.
    //   2. Vertical pass: combine the row above/below's sums with the
    //      current row's sum to get each cell's 8-neighbor count in O(1).
    // Each pass only reads the previous generation and writes a disjoint
    // row range, so every row-chunk can run on its own worker thread with
    // no locking.
    auto update = [&] {
        parallel_for_rows(kRows, [&](int r0, int r1) {
            for (int r = r0; r < r1; ++r) {
                for (int c = 0; c < kCols; ++c) {
                    int left  = wrap(c - 1, kCols);
                    int right = wrap(c + 1, kCols);
                    hsum[idx(r, c)] = front.alive[idx(r, left)] +
                                       front.alive[idx(r, c)] +
                                       front.alive[idx(r, right)];
                }
            }
        });

        parallel_for_rows(kRows, [&](int r0, int r1) {
            // Distinct RNG per worker so birth colors don't require locking.
            std::mt19937 rng{std::random_device{}() ^
                              static_cast<unsigned>(r0 * 2654435761u)};
            for (int r = r0; r < r1; ++r) {
                int up   = wrap(r - 1, kRows);
                int down = wrap(r + 1, kRows);
                for (int c = 0; c < kCols; ++c) {
                    int self_i = idx(r, c);
                    int neighbors = hsum[idx(up, c)] + hsum[idx(down, c)] +
                                     (hsum[self_i] - front.alive[self_i]);
                    bool was_alive = front.alive[self_i];
                    bool now_alive = was_alive ? (neighbors == 2 || neighbors == 3)
                                                : (neighbors == 3);
                    back.alive[self_i] = now_alive;
                    if (now_alive) {
                        back.color[self_i] =
                            was_alive ? front.color[self_i] : RandomColor(rng);
                    }
                }
            }
        });

        std::swap(front, back);
    };

    // Renders two grid rows per terminal row using the upper-half-block glyph
    // (foreground = top cell's color, background = bottom cell's color). This
    // packs the 100x100 grid into 100x50 terminal cells; since terminal
    // character cells are roughly twice as tall as they are wide, each unit
    // ends up rendered as an approximately square block.
    auto renderer = Renderer([&] {
        Elements rows;
        rows.reserve(kRows / 2);
        for (int r = 0; r < kRows; r += 2) {
            Elements cells;
            cells.reserve(kCols);
            for (int c = 0; c < kCols; ++c) {
                int top_i = idx(r, c);
                int bot_i = idx(r + 1, c);
                Color fg = front.alive[top_i] ? front.color[top_i] : Color::Black;
                Color bg = front.alive[bot_i] ? front.color[bot_i] : Color::Black;
                cells.push_back(text("\xE2\x96\x80") | color(fg) | bgcolor(bg));
            }
            rows.push_back(hbox(std::move(cells)));
        }
        return vbox(std::move(rows));
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            update();
            return false;  // let the renderer run after the state update
        }
        if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    // Timer thread: fires every second and posts a Custom event to the main
    // loop, which triggers update() above.
    std::atomic<bool> running{true};
    std::thread timer([&] {
        while (running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(kTickInterval);
            if (running.load(std::memory_order_relaxed))
                screen.PostEvent(Event::Custom);
        }
    });

    screen.Loop(app);

    running.store(false, std::memory_order_relaxed);
    timer.join();
}
