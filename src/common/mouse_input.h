#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>

namespace common {

// Event::mouse() is non-const in FTXUI, so this must take Event by value (or
// non-const reference) -- matching how every CatchEvent handler in this repo
// already receives its Event parameter.
inline bool IsLeftClickPress(ftxui::Event e) {
    return e.is_mouse() && e.mouse().button == ftxui::Mouse::Left && e.mouse().motion == ftxui::Mouse::Pressed;
}

}  // namespace common
