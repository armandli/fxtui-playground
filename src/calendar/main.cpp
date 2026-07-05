#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include <ctime>
#include <iostream>
#include <string>

using namespace ftxui;

namespace {

struct YMD {
  int year, month, day;
};

YMD today() {
  std::time_t t  = std::time(nullptr);
  std::tm*    tm = std::localtime(&t);
  return {tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday};
}

int days_in_month(int year, int month) {
  if (month == 2) {
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return days[month];
}

// Returns 0=Sunday … 6=Saturday for the 1st of the given year/month.
int first_weekday(int year, int month) {
  std::tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = 1;
  std::mktime(&t);
  return t.tm_wday;
}

const char* month_name(int month) {
  static const char* names[] = {
      "", "January", "February", "March",    "April",   "May",      "June",
      "July", "August",   "September", "October", "November", "December"};
  return names[month];
}

}  // namespace

// Display a calendar for the month/year encoded in date_int (format YYYYMMDD).
// Today's cell receives a highlighted border only when today falls within the
// displayed month; otherwise all cells use the same default border.
void display_calendar(int date_int) {
  const int year  = date_int / 10000;
  const int month = (date_int / 100) % 100;

  const YMD now       = today();
  const int num_days  = days_in_month(year, month);
  const int first_dow = first_weekday(year, month);       // 0 = Sunday
  const int num_rows  = (first_dow + num_days + 6) / 7;  // weeks needed

  static const char* dow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const std::string  title = std::string(month_name(month)) + " " + std::to_string(year);

  // ── Day-of-week header ──────────────────────────────────────────────────────
  // size(WIDTH, GREATER_THAN, 5) gives each header cell min-width 5, matching
  // the bordered day cells so all rows share the same flex distribution.
  Elements dow_cells;
  for (int d = 0; d < 7; ++d)
    dow_cells.push_back(text(dow[d]) | bold | center | size(WIDTH, GREATER_THAN, 5) | flex);

  // ── Calendar grid ───────────────────────────────────────────────────────────
  Elements grid_rows;
  for (int row = 0; row < num_rows; ++row) {
    Elements row_cells;
    for (int col = 0; col < 7; ++col) {
      const int day_num = row * 7 + col - first_dow + 1;

      if (day_num < 1 || day_num > num_days) {
        // Uniform min-width 5 keeps flex distribution equal to filled cells.
        row_cells.push_back(
            filler() | size(HEIGHT, EQUAL, 4) | size(WIDTH, GREATER_THAN, 5) | flex);
        continue;
      }

      const bool highlight =
          now.year == year && now.month == month && now.day == day_num;

      // 3-char label ("  1"…"  9" or " 10"…" 31") makes text min-width 3, so
      // bordered cell min-width = 3 + 2 borders = 5 for all days.
      const std::string label =
          day_num < 10 ? "  " + std::to_string(day_num) : " " + std::to_string(day_num);

      auto inner = vbox({
                       hbox({text(label), filler()}),
                       filler(),
                   }) |
                   size(HEIGHT, EQUAL, 4);

      Element cell = highlight ? inner | borderStyled(LIGHT, Color::YellowLight)
                               : inner | borderStyled(LIGHT);

      row_cells.push_back(cell | flex);
    }
    grid_rows.push_back(hbox(row_cells));
  }

  // ── Full document ───────────────────────────────────────────────────────────
  auto document = vbox({
      hbox({filler(), text(title) | bold}),
      separator(),
      hbox(dow_cells),
      separator(),
      vbox(grid_rows),
  });

  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();
  std::cout << "\n";
}

int main() {
  display_calendar(20260702);
  return 0;
}
