#include "log.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>

namespace fa::log {

namespace {
std::mutex g_mutex;
std::ofstream g_file;
std::filesystem::path g_directory;
} // namespace

void open(const std::filesystem::path& file) {
   std::scoped_lock lock(g_mutex);
   g_file.open(file, std::ios::out | std::ios::trunc | std::ios::binary);
   g_directory = file.parent_path();
}

const std::filesystem::path& directory() { return g_directory; }

void write(std::string_view level, std::string_view message) {
   std::scoped_lock lock(g_mutex);
   auto now = std::chrono::zoned_time(std::chrono::current_zone(),
                                      std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
   auto line = std::format("{:%H:%M:%S} [{}] {}\n", now, level, message);
   // Tools that never open a log file get it on stderr.
   if (!g_file.is_open()) {
      std::fputs(line.c_str(), stderr);
      return;
   }
   g_file << line;
   g_file.flush();
}

} // namespace fa::log
