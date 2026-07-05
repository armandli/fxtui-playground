#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <deque>
#include <map>
#include <string>

using namespace ftxui;

namespace {

struct Message {
    std::string username;
    std::string text;
};

const std::vector<Color> kUserColors = {
    Color::Cyan,
    Color::Green,
    Color::YellowLight,
    Color::MagentaLight,
    Color::BlueLight,
    Color::Red,
    Color::CyanLight,
    Color::GreenLight,
};

std::map<std::string, Color> g_user_colors;
int g_next_color = 0;

Color user_color(const std::string& username) {
    auto [it, inserted] = g_user_colors.emplace(username, Color::Default);
    if (inserted)
        it->second = kUserColors[g_next_color++ % kUserColors.size()];
    return it->second;
}

// Creates a bordered message block with the username in the border title and the
// message body auto-wrapped to fill the available width.
Element make_message_block(const std::string& username, const std::string& msg) {
    Color c = user_color(username);
    return window(text(username) | bold, paragraph(msg)) | color(c);
}

// Rough height estimate for a message block given terminal width.
// Each block = top border (1) + wrapped text lines + bottom border (1).
int estimate_height(const std::string& msg, int terminal_width) {
    int usable = std::max(terminal_width - 4, 1);
    int lines   = (static_cast<int>(msg.size()) + usable - 1) / usable;
    return std::max(lines, 1) + 2;
}

// Removes the oldest messages until all remaining messages fit within
// available_height terminal rows.
void trim_to_fit(std::deque<Message>& messages,
                 int available_height,
                 int terminal_width) {
    int total = 0;
    for (const auto& m : messages)
        total += estimate_height(m.text, terminal_width);
    while (!messages.empty() && total > available_height) {
        total -= estimate_height(messages.front().text, terminal_width);
        messages.pop_front();
    }
}

}  // namespace

int main() {
    // Pre-register colours in a stable order so each username always gets the
    // same colour regardless of which messages are currently visible.
    user_color("alice");
    user_color("bob");
    user_color("charlie");
    user_color("dave");
    user_color("you");

    std::deque<Message> messages = {
        {"alice",   "Hey everyone! How's it going?"},
        {"bob",     "Pretty good, just shipped the new feature!"},
        {"charlie", "Nice! What did you end up using for the async layer?"},
        {"alice",   "We went with Rust for the backend and React on the front end. Took a while but the performance is great."},
        {"dave",    "Have you benchmarked it against the old Go service?"},
        {"bob",     "Yeah, latency is down about 40%. The lifetime borrow checker was painful at first but pays off."},
        {"you",     "Did you run into any issues with error propagation across async boundaries?"},
        {"alice",   "Tons! anyhow + thiserror saved us. Highly recommend that combo."},
    };

    std::string input_text;
    auto screen = ScreenInteractive::Fullscreen();

    InputOption opt;
    opt.multiline = false;
    opt.on_enter  = [&] {
        if (input_text.empty())
            return;
        messages.push_back({"you", input_text});
        input_text.clear();
        auto dims = Terminal::Size();
        // Reserve 3 rows for the input window + 1 safety margin.
        trim_to_fit(messages, dims.dimy - 4, dims.dimx);
    };

    auto input_comp = Input(&input_text, "Type a message and press Enter…", opt);

    auto app = Renderer(input_comp, [&] {
        Elements blocks;
        blocks.reserve(messages.size());
        for (const auto& m : messages)
            blocks.push_back(make_message_block(m.username, m.text));

        Element msg_area = blocks.empty() ? filler() : vbox(std::move(blocks));

        Color you_color = user_color("you");
        return vbox({
            msg_area | yflex,
            window(
                text("you") | bold | color(you_color),
                input_comp->Render() | flex
            ) | color(you_color) | size(HEIGHT, EQUAL, 3),
        });
    });

    screen.Loop(app);
}
