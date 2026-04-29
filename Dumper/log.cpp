#include "log.h"
#include <cstdio>
#include <ctime>

static FILE* gLogFile = nullptr;

void LogInit(const std::string& path) {
    fopen_s(&gLogFile, path.c_str(), "w");
}

void LogClose() {
    if (gLogFile) { fclose(gLogFile); gLogFile = nullptr; }
}

void _LogWrite(const char* level, const std::string& msg) {
    fputs(msg.c_str(), stdout);

    if (!gLogFile) return;

    std::string clean = msg;
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r'))
        clean.pop_back();

    time_t t = time(nullptr);
    struct tm lt{};
    localtime_s(&lt, &t);
    char ts[12];
    strftime(ts, sizeof(ts), "%H:%M:%S", &lt);
    fprintf(gLogFile, "[%s] [%-5s] %s\n", ts, level, clean.c_str());
    fflush(gLogFile);
}
