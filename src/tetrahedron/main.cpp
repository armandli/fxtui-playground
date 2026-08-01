#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

#include "camera.h"
#include "mesh.h"
#include "pipeline.h"
#include "scene.h"
#include "transform.h"

using namespace ftxui;

namespace {

constexpr double kSpeed = 0.6;  // canvas dots per tick
constexpr auto kTickInterval = std::chrono::milliseconds(33);

struct CanvasSize {
    int w, h;
};

// Overhead: title(1) + separator(1) + border top/bottom(2) = 4, matching
// rotating_triangle's accounting; single source of truth so the renderer
// and the bounce physics always agree on the play field's size.
CanvasSize ComputeCanvasSize(Dimensions term) {
    int avail_rows = std::max(4, term.dimy - 4);
    int avail_cols = std::max(10, term.dimx - 2);
    return {avail_cols * 2, avail_rows * 4};
}

}  // namespace

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    Angles angles;

    std::mt19937 rng{std::random_device{}()};
    CanvasSize initial = ComputeCanvasSize(Terminal::Size());
    Scene scene = InitScene(rng, initial.w / 2.0, initial.h / 2.0, kSpeed);

    auto renderer = Renderer([&] {
        CanvasSize size = ComputeCanvasSize(Terminal::Size());
        auto draw_fn = [&](Canvas& c) { RenderFrame(angles, scene, c); };
        auto canvas_elem = canvas(size.w, size.h, draw_fn) | border | flex;

        return vbox({
            text("Tetrahedron — press q to quit") | bold | center,
            separator(),
            canvas_elem,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            Advance(angles);

            CanvasSize size = ComputeCanvasSize(Terminal::Size());
            double scale = NominalScale(size.w, size.h);
            UpdateScene(scene, size.w, size.h, scale * kCircumradius);

            return false;  // let the renderer run after the state update
        }
        if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    // Timer thread: fires every ~33 ms (~30 FPS) and posts a Custom event to the main loop.
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
