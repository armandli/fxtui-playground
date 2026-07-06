#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace ftxui;

int main(int argc, char* argv[]) {
    std::string filepath;
    std::string content;

    if (argc > 1) {
        filepath = argv[1];
        std::ifstream file(filepath);
        if (file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            content = ss.str();
        }
    }

    const std::string display_name = filepath.empty() ? "[No File]" : filepath;
    bool modified = false;

    auto screen = ScreenInteractive::Fullscreen();

    InputOption opt;
    opt.multiline  = true;
    opt.on_change  = [&] { modified = true; };

    auto input_comp = Input(&content, "", opt);

    auto save_file = [&] {
        if (!filepath.empty()) {
            std::ofstream file(filepath);
            if (file.is_open()) {
                file << content;
                modified = false;
            }
        }
    };

    auto app = Renderer(input_comp, [&] {
        std::string title = " " + display_name + (modified ? " *" : "") + " ";

        auto editor = window(
            text(title) | color(Color::Green),
            input_comp->Render()
                | color(Color::Green)
                | bgcolor(Color::Black)
                | flex
        ) | color(Color::Green) | flex;

        auto status = hbox({
            text("  Ctrl+S ") | bold | color(Color::Green),
            text("Save   ") | color(Color::Green),
            text("Ctrl+Q ") | bold | color(Color::Green),
            text("Quit  ") | color(Color::Green),
        });

        return vbox({editor | flex, status});
    });

    auto with_events = CatchEvent(app, [&](Event e) -> bool {
        if (e == Event::Special("\x13")) {  // Ctrl+S
            save_file();
            return true;
        }
        if (e == Event::Special("\x11")) {  // Ctrl+Q
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(with_events);
}
