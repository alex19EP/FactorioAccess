#pragma once

#include <filesystem>
#include <format>
#include <string_view>

namespace fa::log {

void open(const std::filesystem::path& file);
void write(std::string_view level, std::string_view message);

template <class... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
   write("info", std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
   write("warn", std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
   write("error", std::format(fmt, std::forward<Args>(args)...));
}

} // namespace fa::log
