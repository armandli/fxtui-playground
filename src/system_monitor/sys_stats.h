#ifndef SYS_STATS_H
#define SYS_STATS_H

#include <cstdint>

#include <vector>

// Host CPU / memory / network counters, behind one interface with two
// backends: Mach host_* calls on macOS, and the /proc pseudo-filesystem on
// Linux. Every reader returns false rather than throwing when the underlying
// source is unavailable (missing /proc entry, denied Mach port), so callers
// can probe once at startup and degrade gracefully.
//
// All three readers report *cumulative* counters -- callers take deltas
// between two samples to get rates.

namespace sysstat {

// Cumulative per-CPU jiffies/ticks. Units are platform-defined and only
// meaningful as ratios within one sample's total.
struct CpuTicks { uint64_t user{}, sys{}, idle{}, nice{}; };

// One entry per logical CPU, in kernel order.
inline bool read_cpu_ticks(std::vector<CpuTicks>& out);

// Physical memory in use, as a percentage of total.
inline bool read_mem_pct(double& pct);

// Sum of bytes in + bytes out across all non-loopback interfaces.
inline bool read_net_bytes(uint64_t& total);

}  // namespace sysstat

// ─── macOS backend (Mach) ────────────────────────────────────────────────────
#if defined(__APPLE__)

#include <ifaddrs.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <net/if.h>
#include <net/if_var.h>
#include <sys/socket.h>
#include <unistd.h>

namespace sysstat {

inline bool read_cpu_ticks(std::vector<CpuTicks>& out) {
  processor_info_array_t info = nullptr;
  mach_msg_type_number_t info_cnt = 0;
  natural_t cpu_cnt = 0;

  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
          &cpu_cnt, &info, &info_cnt) != KERN_SUCCESS)
    return false;

  auto* base = reinterpret_cast<processor_cpu_load_info_data_t*>(info);
  out.resize(cpu_cnt);
  for (natural_t i = 0; i < cpu_cnt; ++i) {
    out[i].user = base[i].cpu_ticks[CPU_STATE_USER];
    out[i].sys = base[i].cpu_ticks[CPU_STATE_SYSTEM];
    out[i].idle = base[i].cpu_ticks[CPU_STATE_IDLE];
    out[i].nice = base[i].cpu_ticks[CPU_STATE_NICE];
  }
  vm_deallocate(
      mach_task_self(),
      reinterpret_cast<vm_address_t>(info),
      static_cast<vm_size_t>(info_cnt) * sizeof(integer_t));
  return true;
}

inline bool read_mem_pct(double& pct) {
  host_basic_info_data_t basic{};
  mach_msg_type_number_t basic_cnt = HOST_BASIC_INFO_COUNT;
  if (host_info(mach_host_self(), HOST_BASIC_INFO,
          reinterpret_cast<host_info_t>(&basic), &basic_cnt) != KERN_SUCCESS)
    return false;

  vm_statistics64_data_t vm{};
  mach_msg_type_number_t vm_cnt = HOST_VM_INFO64_COUNT;
  if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
          reinterpret_cast<host_info64_t>(&vm), &vm_cnt) != KERN_SUCCESS)
    return false;

  long page_sz = sysconf(_SC_PAGESIZE);
  if (page_sz <= 0 or basic.max_mem == 0) return false;

  uint64_t used = (static_cast<uint64_t>(vm.active_count) +
                   static_cast<uint64_t>(vm.wire_count) +
                   static_cast<uint64_t>(vm.compressor_page_count)) *
                  static_cast<uint64_t>(page_sz);
  pct =
      static_cast<double>(used) / static_cast<double>(basic.max_mem) * 100.0;
  return true;
}

// Sums ibytes + obytes across all non-loopback UP AF_LINK interfaces.
inline bool read_net_bytes(uint64_t& total) {
  struct ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) return false;
  total = 0;
  for (auto* ifa = list; ifa; ifa = ifa->ifa_next) {
    if (not ifa->ifa_addr or ifa->ifa_addr->sa_family != AF_LINK) continue;
    if (ifa->ifa_flags & IFF_LOOPBACK) continue;
    if (not (ifa->ifa_flags & IFF_UP)) continue;
    if (not ifa->ifa_data) continue;
    auto* d = reinterpret_cast<struct if_data*>(ifa->ifa_data);
    total += static_cast<uint64_t>(d->ifi_ibytes) +
             static_cast<uint64_t>(d->ifi_obytes);
  }
  freeifaddrs(list);
  return true;
}

}  // namespace sysstat

// ─── Linux backend (/proc) ───────────────────────────────────────────────────
#elif defined(__linux__)

#include <cctype>

#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace sysstat::detail {

constexpr const char* kProcStat = "/proc/stat";
constexpr const char* kProcMeminfo = "/proc/meminfo";
constexpr const char* kProcNetDev = "/proc/net/dev";

}  // namespace sysstat::detail

namespace sysstat {

// /proc/stat carries one "cpu" aggregate line followed by one "cpuN" line per
// logical CPU:
//
//   cpuN user nice system idle iowait irq softirq steal guest guest_nice
//
// The aggregate line is skipped (it is the sum of the rest). iowait folds
// into idle, and the interrupt/steal buckets fold into system, so the four
// fields carry the same meaning as their Mach counterparts.
inline bool read_cpu_ticks(std::vector<CpuTicks>& out) {
  std::ifstream in(detail::kProcStat);
  if (not in) return false;

  out.clear();
  std::string line;
  while (std::getline(in, line)) {
    if (line.compare(0, 3, "cpu") != 0) break;  // per-CPU lines come first
    if (line.size() < 4 or
        not std::isdigit(static_cast<unsigned char>(line[3])))
      continue;  // the "cpu " aggregate line

    std::istringstream ls(line);
    std::string label;
    uint64_t user = 0, nice = 0, system = 0, idle = 0;
    uint64_t iowait = 0, irq = 0, softirq = 0, steal = 0;
    if (not (ls >> label >> user >> nice >> system >> idle)) return false;
    // Trailing fields arrived over successive kernel versions; a failed
    // extraction leaves the target zeroed, which is the right default.
    ls >> iowait >> irq >> softirq >> steal;

    out.push_back(CpuTicks{
        .user = user,
        .sys = system + irq + softirq + steal,
        .idle = idle + iowait,
        .nice = nice,
    });
  }
  return not out.empty();
}

// /proc/meminfo reports kB values. MemAvailable is the kernel's own estimate
// of allocatable memory (reclaimable cache included), which is the closest
// analogue to what the Mach path reports as "not in use".
inline bool read_mem_pct(double& pct) {
  std::ifstream in(detail::kProcMeminfo);
  if (not in) return false;

  uint64_t total_kb = 0, available_kb = 0, free_kb = 0;
  bool have_available = false;
  std::string key;
  uint64_t value = 0;
  while (in >> key >> value) {
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (key == "MemTotal:") {
      total_kb = value;
    } else if (key == "MemAvailable:") {
      available_kb = value;
      have_available = true;
    } else if (key == "MemFree:") {
      free_kb = value;
    }
    if (total_kb and have_available) break;
  }
  if (total_kb == 0) return false;
  if (not have_available) available_kb = free_kb;  // pre-3.14 kernels

  uint64_t used_kb = available_kb < total_kb ? total_kb - available_kb : 0;
  pct = static_cast<double>(used_kb) / static_cast<double>(total_kb) * 100.0;
  return true;
}

// /proc/net/dev has two header lines, then one line per interface:
//
//   iface: rx_bytes rx_packets ... (8 rx fields) tx_bytes tx_packets ...
//
// Loopback is excluded to match the macOS backend.
inline bool read_net_bytes(uint64_t& total) {
  std::ifstream in(detail::kProcNetDev);
  if (not in) return false;

  std::string line;
  std::getline(in, line);  // "Inter-|   Receive ..."
  std::getline(in, line);  // " face |bytes packets ..."

  total = 0;
  while (std::getline(in, line)) {
    auto colon = line.find(':');
    if (colon == std::string::npos) continue;

    std::string name = line.substr(0, colon);
    auto first = name.find_first_not_of(" \t");
    if (first == std::string::npos) continue;
    name = name.substr(first, name.find_last_not_of(" \t") - first + 1);
    if (name == "lo") continue;

    std::istringstream ls(line.substr(colon + 1));
    uint64_t rx_bytes = 0, tx_bytes = 0, skip = 0;
    ls >> rx_bytes;
    for (int i = 0; i < 7 and ls; ++i) ls >> skip;  // rest of the rx block
    ls >> tx_bytes;
    if (not ls) continue;  // truncated row -- ignore this interface

    total += rx_bytes + tx_bytes;
  }
  return true;
}

}  // namespace sysstat

#else
#error "system_monitor supports macOS (Mach) and Linux (/proc) only"
#endif

#endif
