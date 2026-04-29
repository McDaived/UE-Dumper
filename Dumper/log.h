#pragma once
#include <string>
#include <fmt/core.h>

void LogInit(const std::string& path);
void LogClose();
void _LogWrite(const char* level, const std::string& msg);

#define LOG(...)  _LogWrite("INFO",  fmt::format(__VA_ARGS__))
#define LOGW(...) _LogWrite("WARN",  fmt::format(__VA_ARGS__))
#define LOGE(...) _LogWrite("ERROR", fmt::format(__VA_ARGS__))
