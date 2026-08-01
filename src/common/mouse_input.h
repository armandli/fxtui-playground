#ifndef MOUSE_INPUT_H
#define MOUSE_INPUT_H

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>

namespace common {

// Event::mouse() is non-const in FTXUI, so this must take Event by value (or
// non-const reference) -- matching how every CatchEvent handler in this repo
// already receives its Event parameter.
inline bool is_left_click_press(ftxui::Event e) {
  return e.is_mouse()
      and e.mouse().button == ftxui::Mouse::Left
      and e.mouse().motion == ftxui::Mouse::Pressed;
}

}  // namespace common

#endif  // MOUSE_INPUT_H
