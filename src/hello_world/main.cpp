#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>

int main() {
  using namespace ftxui;

  auto document = hbox({
    text("Hello,") | bold,
    text(" "),
    text("World!") | color(Color::Green),
  });

  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();
  std::cout << "\n";
}
