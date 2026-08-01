#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <sys_stats.h>

using namespace ftxui;

using sysstat::CpuTicks;
using sysstat::read_cpu_ticks;
using sysstat::read_mem_pct;
using sysstat::read_net_bytes;

namespace {

constexpr int kHistoryLen = 120;   // data points kept per graph (~6 min at 3 s)
constexpr int kSampleMs   = 3000;  // sampling interval (ms)
constexpr int kPollMs     = 50;    // sleep granularity for quit check

const std::vector<Color> kCpuPalette = {
    Color::Cyan,
    Color::Green,
    Color::YellowLight,
    Color::MagentaLight,
    Color::BlueLight,
    Color::Red,
    Color::CyanLight,
    Color::GreenLight,
};

// ─── History (shared, mutex-protected) ───────────────────────────────────────
struct History {
  std::vector<std::deque<double>> cpu;  // per-CPU busy %
  std::deque<double> mem;  // memory usage %
  std::deque<double> net;  // raw KB/s
  double net_max = 1.0;  // max observed KB/s (for scaling)
  mutable std::mutex mtx;
};

template <typename T>
void push_capped(std::deque<T>& d, T v) {
  d.push_back(v);
  if (static_cast<int>(d.size()) > kHistoryLen) d.pop_front();
}

// ─── Section border helper ───────────────────────────────────────────────────
// Produces a rounded, colored border around `content` with `title` embedded
// in the top border line using a dbox overlay trick:
//   • Bottom layer: content with ROUNDED border in color c
//   • Top layer:    1-line hbox that redraws the "╭" corner and writes the
//                   title; filler() is transparent so remaining "─" chars
//                   show through.
Element make_section(const std::string& title, Color c, Element content) {
  return dbox({
      std::move(content) | borderStyled(ROUNDED) | color(c),
      hbox({
          text("╭") | color(c),
          text(" " + title + " ") | bold | color(c),
          filler(),
      }),
  });
}

// ─── Graph function builder ─────────────────────────────────────────────────
// Returned GraphFunction is safe to call from any thread (locks hist.mtx).
// `get_d`  : selects the deque to read from History
// `scale`  : maps a raw deque value to [0.0, 1.0]
GraphFunction make_graph(
    const History& hist,
    std::function<const std::deque<double>&(const History&)> get_d,
    std::function<double(double, const History&)> scale)
{
  return [&hist, get_d = std::move(get_d), scale = std::move(scale)](
      int w, int h) -> std::vector<int> {
    std::lock_guard lock(hist.mtx);
    const auto& d = get_d(hist);
    std::vector<int> out(w, 0);
    int base = std::max(0, static_cast<int>(d.size()) - w);
    for (int x = 0; x < w; ++x) {
      int di = base + x;
      if (di < static_cast<int>(d.size())) {
        double p = scale(d[di], hist);
        out[x] = static_cast<int>(std::clamp(p, 0.0, 1.0) * (h - 1));
      }
    }
    return out;
  };
}

}  // namespace

int main() {
  // ─── Probe APIs (before any FTXUI initialisation) ──────────────────────
  {
    std::vector<CpuTicks> probe;
    if (not read_cpu_ticks(probe) or probe.empty()) {
      std::cerr << "Error: cannot read CPU load information\n";
      return 1;
    }
    double probe_mem = 0.0;
    if (not read_mem_pct(probe_mem)) {
      std::cerr << "Error: cannot read memory information\n";
      return 1;
    }
    uint64_t probe_net = 0;
    if (not read_net_bytes(probe_net)) {
      std::cerr << "Error: cannot read network byte counters\n";
      return 1;
    }
  }

  // ─── Initial state for delta calculation ────────────────────────────────
  std::vector<CpuTicks> prev_cpu;
  read_cpu_ticks(prev_cpu);
  int num_cpus = static_cast<int>(prev_cpu.size());

  uint64_t prev_net = 0;
  read_net_bytes(prev_net);

  // ─── CPU grid geometry ───────────────────────────────────────────────────
  int grid_cols = static_cast<int>(
      std::ceil(std::sqrt(static_cast<double>(num_cpus))));
  int grid_rows = (num_cpus + grid_cols - 1) / grid_cols;

  // ─── Shared history ──────────────────────────────────────────────────────
  History history;
  history.cpu.resize(num_cpus);

  // ─── FTXUI screen ────────────────────────────────────────────────────────
  auto screen = ScreenInteractive::Fullscreen();
  std::atomic<bool> quit{false};

  // ─── Sampling thread (every kSampleMs, posts Custom event to trigger
  // redraw)
  std::thread sampler([&] {
    while (not quit.load(std::memory_order_relaxed)) {
      // --- CPU ---
      std::vector<CpuTicks> cur;
      if (read_cpu_ticks(cur) and
          static_cast<int>(cur.size()) >= num_cpus) {
        std::lock_guard lock(history.mtx);
        for (int i = 0; i < num_cpus; ++i) {
          uint64_t du = cur[i].user - prev_cpu[i].user;
          uint64_t ds = cur[i].sys - prev_cpu[i].sys;
          uint64_t dd = cur[i].idle - prev_cpu[i].idle;
          uint64_t dn = cur[i].nice - prev_cpu[i].nice;
          uint64_t total = du + ds + dd + dn;
          double pct = total > 0
              ? static_cast<double>(du + ds + dn) / total * 100.0
              : 0.0;
          push_capped(history.cpu[i], pct);
        }
        prev_cpu = cur;
      }

      // --- Memory ---
      double mem_pct = 0.0;
      if (read_mem_pct(mem_pct)) {
        std::lock_guard lock(history.mtx);
        push_capped(history.mem, mem_pct);
      }

      // --- Network ---
      uint64_t cur_net = 0;
      if (read_net_bytes(cur_net) and cur_net >= prev_net) {
        double kbps = static_cast<double>(cur_net - prev_net) /
            (kSampleMs / 1000.0) / 1024.0;
        prev_net = cur_net;
        std::lock_guard lock(history.mtx);
        if (kbps > history.net_max) history.net_max = kbps;
        push_capped(history.net, kbps);
      }

      screen.PostEvent(Event::Custom);

      // Sleep in small chunks so we can react to quit quickly.
      for (int i = 0; i < kSampleMs / kPollMs and not quit.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    }
  });

  // ─── Renderer ────────────────────────────────────────────────────────────
  auto app = Renderer([&] {
    // Snapshot titles under lock (graph functions lock independently when
    // called).
    std::vector<double> cpu_now(num_cpus, 0.0);
    double mem_now = 0.0, net_now = 0.0, net_max_now = 1.0;
    {
      std::lock_guard lock(history.mtx);
      for (int i = 0; i < num_cpus; ++i)
        if (not history.cpu[i].empty()) cpu_now[i] = history.cpu[i].back();
      if (not history.mem.empty()) mem_now = history.mem.back();
      if (not history.net.empty()) net_now = history.net.back();
      net_max_now = history.net_max;
    }

    // ── CPU grid ─────────────────────────────────────────────────────────
    Elements rows;
    for (int r = 0; r < grid_rows; ++r) {
      Elements cells;
      for (int c = 0; c < grid_cols; ++c) {
        int idx = r * grid_cols + c;
        if (idx < num_cpus) {
          Color col = kCpuPalette[idx % kCpuPalette.size()];
          std::string title = std::format(
              "CPU {:d}: {:.1f}%", idx, cpu_now[idx]);
          GraphFunction fn = make_graph(
              history,
              [idx](const History& h) -> const std::deque<double>& {
                return h.cpu[idx];
              },
              [](double v, const History&) { return v / 100.0; });
          cells.push_back(
              window(text(title) | bold, graph(fn)) | color(col) | flex);
        } else {
          cells.push_back(filler() | flex);
        }
      }
      rows.push_back(hbox(cells) | flex);
    }

    // ── Memory graph ─────────────────────────────────────────────────────
    GraphFunction mem_fn = make_graph(
        history,
        [](const History& h) -> const std::deque<double>& { return h.mem; },
        [](double v, const History&) { return v / 100.0; });

    // ── Network graph (auto-rescales to max observed KB/s) ──────────────
    GraphFunction net_fn = make_graph(
        history,
        [](const History& h) -> const std::deque<double>& { return h.net; },
        [](double v, const History& h) {
          return h.net_max > 0.0 ? v / h.net_max : 0.0;
        });

    std::string mem_title = std::format("Memory  {:.1f}%", mem_now);
    std::string net_title = std::format(
        "Network  {:.1f} KB/s   max {:.1f} KB/s", net_now, net_max_now);

    // ── Full layout ──────────────────────────────────────────────────────
    return vbox({
        make_section(
            "CPU Usage", Color::Cyan, vbox(rows) | yflex_grow) | yflex,
        hbox({
            make_section(
                mem_title, Color::Green, graph(mem_fn) | yflex_grow) | flex,
            make_section(
                net_title, Color::YellowLight,
                graph(net_fn) | yflex_grow) | flex,
        }) | size(HEIGHT, GREATER_THAN, 12),
    });
  });

  auto app_with_quit = CatchEvent(app, [&](Event e) -> bool {
    if (e == Event::Character('q') or e == Event::Character('Q')) {
      screen.Exit();
      return true;
    }
    return false;
  });

  screen.Loop(app_with_quit);
  quit.store(true);
  sampler.join();
}
