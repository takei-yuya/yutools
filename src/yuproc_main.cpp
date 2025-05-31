#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <sstream>
#include <vector>
#include <iomanip>
#include <map>
#include <chrono>
#include <algorithm>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include "file_util.hpp"

struct Options {
  bool tree = false;
  bool regular_files = false;
  bool prune = false;
  bool color = false;
  int loop = 0;  // 0 means no loop, >0 means loop for N seconds
};

std::string GetLocalTime() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local_tm;
  localtime_r(&time, &local_tm);
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
  return std::string(buffer);
}

std::string Truncate(const std::string& str, size_t max_length) {
  if (str.size() > max_length) {
    return str.substr(0, max_length - 3) + "...";
  }
  return str;
}

std::string Trim(const std::string& str, const std::string& whitespace = " \t") {
  size_t start = str.find_first_not_of(whitespace);
  if (start == std::string::npos) {
    return "";
  }
  size_t end = str.find_last_not_of(whitespace);
  return str.substr(start, end - start + 1);
}

std::string Quote(const std::string& str) {
  std::ostringstream oss;
  oss << '"';
  for (char c : str) {
    if (c == '"') {
      oss << "\\\"";
    } else if (c == '\\') {
      oss << "\\\\";
    } else {
      oss << c;
    }
  }
  oss << '"';
  return oss.str();
}

std::string ColorBar(const std::string& str, float percent) {
  size_t pos = static_cast<size_t>(str.size() * percent / 100.0);
  return
    "\x1b[42m" + str.substr(0, pos) + "\x1b[0m" +
    "\x1b[42;7m" + str.substr(pos) + "\x1b[0m";
}

std::string ReadAll(std::istream& ifs) {
  std::vector<char> buffer(8 * 1024);
  std::string result;
  while (ifs) {
    ifs.read(buffer.data(), buffer.size());
    result.append(buffer.data(), ifs.gcount());
  }
  return result;
}

std::string HumanReadableSize(size_t size) {
  const std::vector<std::string> kUnits = {"  B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
  size_t unit_index = 0;
  double human_size = static_cast<double>(size);
  for (; unit_index + 1 < kUnits.size(); ++unit_index) {
    if (human_size < 1024.0) {
      break;
    }
    human_size /= 1024.0;
  }
  std::ostringstream oss;
  oss << std::fixed << std::setw(6) << std::setprecision(1) << human_size << kUnits[unit_index];
  return oss.str();
}

std::string ModeToString(mode_t mode) {
  // ________________
  // rw               O_ACCMODE (O_RDONLY, O_WRONLY, O_RDWR)
  //   c              O_CREAT
  //    x             O_EXCL
  //     T            O_NOCTTY
  //      t           O_TRUNC
  //       a          O_APPEND
  //        n         O_NONBLOCK
  //         s        O_DSYNC
  //          f       FASYNC
  //           d      O_DIRECT
  //            l     O_LARGEFILE
  //             Y    O_DIRECTORY
  //              F   O_NOFOLLOW
  //               A  O_NOATIME
  //                e O_CLOEXEC
  std::string mode_str(16, '_');
  if ((mode & O_ACCMODE) == O_RDONLY) {
    mode_str[0] = 'r';
  } else if ((mode & O_ACCMODE) == O_WRONLY) {
    mode_str[1] = 'w';
  } else if ((mode & O_ACCMODE) == O_RDWR) {
    mode_str[0] = 'r';
    mode_str[1] = 'w';
  }
  if (mode & O_CREAT) mode_str[2] = 'c';
  if (mode & O_EXCL) mode_str[3] = 'x';
  if (mode & O_NOCTTY) mode_str[4] = 'T';
  if (mode & O_TRUNC) mode_str[5] = 't';
  if (mode & O_APPEND) mode_str[6] = 'a';
  if (mode & O_NONBLOCK) mode_str[7] = 'n';
  if (mode & O_DSYNC) mode_str[8] = 's';
  if (mode & FASYNC) mode_str[9] = 'f';
  if (mode & O_DIRECT) mode_str[10] = 'd';
  if (mode & O_LARGEFILE) mode_str[11] = 'l';
  if (mode & O_DIRECTORY) mode_str[12] = 'Y';
  if (mode & O_NOFOLLOW) mode_str[13] = 'F';
  if (mode & O_NOATIME) mode_str[14] = 'A';
  if (mode & O_CLOEXEC) mode_str[15] = 'e';

  return mode_str;
}

struct FdInfo {
  int fd;
  std::string path;
  std::string mode;
  ssize_t size;
  size_t pos;
};

struct ProcInfo;
using ProcInfoPtr = std::shared_ptr<ProcInfo>;

struct ProcInfo {
  pid_t pid;
  pid_t ppid;
  std::string cmdline;
  std::vector<FdInfo> fds;
  std::vector<ProcInfoPtr> children;
};

ProcInfoPtr ReadProcFs(pid_t pid, const Options& options) {
  ProcInfoPtr proc_info = std::make_shared<ProcInfo>();
  proc_info->pid = pid;
  {  // read /proc/PID/cmdline
    std::ifstream ifs("/proc/" + std::to_string(pid) + "/cmdline");
    if (ifs) {
      std::string cmdline = ReadAll(ifs);
      for (size_t i = 0; i < cmdline.size(); ++i) {
        if (cmdline[i] == '\0') {
          cmdline[i] = ' ';
        }
      }
      proc_info->cmdline = Trim(cmdline);
    } else {
      return nullptr;
    }
  }
  {  // scan /proc/PID/fd and read /proc/PID/fdinfo/FD
    std::vector<std::string> fds = file_util::ScanDirectory("/proc/" + std::to_string(pid) + "/fd");
    for (const auto& fd : fds) {
      FdInfo fd_info;
      fd_info.fd = std::stoi(fd);
      fd_info.path = file_util::ReadLink("/proc/" + std::to_string(pid) + "/fd/" + fd);
      if (options.regular_files && !file_util::IsFile(fd_info.path)) {
        continue;
      }
      fd_info.size = file_util::GetFileSize(fd_info.path);
      if (fd_info.size < 0) {
        fd_info.size = 0;  // Do not report error, just treat it as size 0
      }
      std::ifstream ifs("/proc/" + std::to_string(pid) + "/fdinfo/" + fd);
      if (ifs) {
        std::string line;
        while (std::getline(ifs, line)) {
          if (line.find("pos:") == 0) {
            fd_info.pos = std::stoull(Trim(line.substr(4)));
          } else if (line.find("flags:") == 0) {
            std::string oct_flags = Trim(line.substr(6));
            std::istringstream iss(oct_flags);
            int flags;
            iss >> std::oct >> flags;
            fd_info.mode = ModeToString(flags);
          }
        }
      } else {
        fd_info.pos = 0;
        fd_info.mode = std::string(16, '?');
      }
      proc_info->fds.push_back(std::move(fd_info));
    }
  }
  {  // read /proc/PID/status
    std::ifstream ifs("/proc/" + std::to_string(pid) + "/status");
    if (ifs) {
      std::string line;
      while (std::getline(ifs, line)) {
        if (line.find("PPid:") == 0) {
          proc_info->ppid = std::stoull(Trim(line.substr(5)));
        }
      }
    } else {
      return nullptr;
    }
  }
  return proc_info;
}

void PrintProcInfo(std::ostream& os, const ProcInfo& proc_info, const Options& options, int indent = 0) {
  std::string indent_str(indent, ' ');
  os << indent_str << "- pid: " << proc_info.pid << std::endl;
  os << indent_str << "  cmd: " << Quote(Truncate(proc_info.cmdline, 100)) << std::endl;
  if (!proc_info.fds.empty()) {
    os << indent_str << "  fds:" << std::endl;
  }
  for (const auto& fd : proc_info.fds) {
    float percent = (fd.size <= 0) ? 0.0 : (static_cast<float>(fd.pos) / fd.size) * 100.0;
    std::ostringstream percent_oss;
    percent_oss << std::fixed << std::setw(5) << std::setprecision(1) << percent;

    std::ostringstream fd_oss;
    fd_oss
      << fd.fd << ": " << fd.mode << " "
      << percent_oss.str() << "% (" << HumanReadableSize(fd.pos) << " / " << HumanReadableSize(fd.size) << ") "
      << fd.path;

    if (options.color) {
      os << indent_str << "    " << ColorBar(fd_oss.str(), percent) << std::endl;
    } else {
      os << indent_str << "    " << fd_oss.str() << std::endl;
    }
  }
  if (!proc_info.children.empty()) {
    os << indent_str << "  children:" << std::endl;
  }
  for (const auto& child : proc_info.children) {
    PrintProcInfo(os, *child, options, indent + 4);
  }
}

std::vector<pid_t> ListUserProcFs() {
  std::vector<pid_t> result;
  for (const auto& proc : file_util::ScanDirectory("/proc")) {
    if (proc.find_first_not_of("0123456789") == std::string::npos && file_util::IsOwned("/proc/" + proc)) {
      result.push_back(std::stoull(proc));
    }
  }
  return result;
}

void Prune(ProcInfo& proc_info) {
  for (auto child : proc_info.children) {
    Prune(*child);
  }
  auto it = std::remove_if(proc_info.children.begin(), proc_info.children.end(),
                         [](const ProcInfoPtr& child) { return child->children.empty() && child->fds.empty(); });
  proc_info.children.erase(it, proc_info.children.end());
}

void Prune(std::map<pid_t, ProcInfoPtr>& proc_map) {
  for (auto it = proc_map.begin(); it != proc_map.end();) {
    Prune(*it->second);
    if (it->second->children.empty() && it->second->fds.empty()) {
      it = proc_map.erase(it);
    } else {
      ++it;
    }
  }
}

void Impl(std::ostream& os, const Options& options, std::vector<pid_t> pids) {
  if (pids.empty()) {
    pids = ListUserProcFs();
  }

  std::map<pid_t, ProcInfoPtr> proc_map;
  for (const auto& pid : pids) {
    ProcInfoPtr proc_info = ReadProcFs(pid, options);
    if (proc_info) {
      proc_map[pid] = proc_info;
    }
  }

  if (options.tree) {
    std::vector<pid_t> moved_pids;
    for (auto& it : proc_map) {
      auto& pid = it.first;
      auto& proc_info = it.second;
      if (proc_map.find(proc_info->ppid) != proc_map.end()) {
        proc_map[proc_info->ppid]->children.push_back(proc_info);
        moved_pids.push_back(pid);
      }
    }
    for (pid_t pid : moved_pids) {
      proc_map.erase(pid);
    }
  }

  if (options.prune) {
    Prune(proc_map);
  }

  for (auto& it : proc_map) {
    PrintProcInfo(os, *it.second, options);
    std::cout << std::endl;
  }
}

void Usage(std::ostream& os, int, char* argv[]) {
  os << "Usage: " << argv[0] << " [PIDS...]" << std::endl;
  os << "" << std::endl;
  os << "Options:" << std::endl;
  os << "  -t, --tree    Show process tree" << std::endl;
  os << "  -r, --regular Show only regular files" << std::endl;
  os << "  -p, --prune   Prune processes with no children or fds" << std::endl;
  os << "  -c, --color   Indicate file offset as a color bar" << std::endl;
  os << "  -l, --loop=N  Loop every N seconds" << std::endl;
  os << "  -h, --help    Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
  Options options;
  while (true) {
    static struct option long_options[] = {
      { "tree", no_argument, nullptr, 't' },
      { "regular", no_argument, nullptr, 'r' },
      { "prune", no_argument, nullptr, 'p' },
      { "color", no_argument, nullptr, 'c' },
      { "loop", required_argument, nullptr, 'l' },
      { "help", no_argument, nullptr, 'h' },
      { nullptr, 0, nullptr, 0 },
    };
    int option_index = 0;
    int c = getopt_long(argc, argv, "trpcl:h", long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 't':
        options.tree = true;
        break;

      case 'r':
        options.regular_files = true;
        break;

      case 'p':
        options.prune = true;
        break;

      case 'c':
        options.color = true;
        break;

      case 'l':
        try {
          options.loop = std::stoi(optarg);
          if (options.loop < 0) {
            std::cerr << "Invalid loop value: " << options.loop << ", must be non-negative." << std::endl;
            return 1;
          }
        } catch (const std::invalid_argument&) {
          std::cerr << "Invalid loop value: " << optarg << ", not a number." << std::endl;
          return 1;
        } catch (const std::out_of_range&) {
          std::cerr << "Invalid loop value: " << optarg << ", out of range." << std::endl;
          return 1;
        }
        break;

      case 'h':
        Usage(std::cout, argc, argv);
        return 0;

      default:
        Usage(std::cerr, argc, argv);
        return 1;
    }
  }
  std::vector<pid_t> args;
  for (int i = optind; i < argc; ++i) {
    try {
      args.push_back(std::stoll(argv[i]));
    } catch (const std::invalid_argument&) {
      std::cerr << "Invalid PID: " << argv[i] << ", not a number." << std::endl;
      return 1;
    } catch (const std::out_of_range&) {
      std::cerr << "Invalid PID: " << argv[i] << ", out of range." << std::endl;
      return 1;
    }
  }

  if (options.loop > 0) {
    while (true) {
      std::cout << "\x1b[2J\x1b[H";
      std::cout << "# " << GetLocalTime() << std::endl << std::endl;
      Impl(std::cout, options, args);
      sleep(options.loop);
    }
  } else {
    Impl(std::cout, options, args);
  }
}
