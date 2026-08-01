#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "pipeline.h"
#include "scene.h"
#include "sphere_field.h"
#include "transform.h"

using namespace ftxui;

namespace {

constexpr double kSpeed = 0.5;  // physics units per tick
constexpr auto kTickInterval = std::chrono::milliseconds(33);

struct GridSize {
    int cols, rows;
};

// Overhead: title(1) + separator(1) + border top/bottom(2) + border sides(2).
GridSize ComputeGridSize(Dimensions term) {
    int cols = std::max(10, term.dimx - 2);
    int rows = std::max(4, term.dimy - 4);
    return {cols, rows};
}

}  // namespace

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    std::mt19937 rng{std::random_device{}()};
    Angles angles;
    RotationRates rates = InitRotationRates(rng);
    LightState light;

    GridSize initial = ComputeGridSize(Terminal::Size());
    Scene scene = InitScene(rng, initial.cols / 2.0, initial.rows, kSpeed);

    auto renderer = Renderer([&] {
        GridSize size = ComputeGridSize(Terminal::Size());
        std::vector<char> chars = RenderFrame(angles, scene, light, size.cols, size.rows);

        Elements rows;
        rows.reserve(size.rows);
        for (int r = 0; r < size.rows; ++r) {
            Elements cells;
            cells.reserve(size.cols);
            for (int c = 0; c < size.cols; ++c) {
                cells.push_back(text(std::string(1, chars[r * size.cols + c])));
            }
            rows.push_back(hbox(std::move(cells)));
        }
        auto grid_elem = vbox(std::move(rows)) | border | flex;

        return vbox({
            text("Sphere — press q to quit") | bold | center,
            separator(),
            grid_elem,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            Advance(angles, rates);
            AdvanceLight(light, rng);

            GridSize size = ComputeGridSize(Terminal::Size());
            double radius = ComputeRadius(size.cols, size.rows);
            UpdateScene(scene, size.cols, size.rows * 2, radius);

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
