#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <cstdio>
#include <fmt/core.h>

class ProgressBar {
    uint64_t total_;
    uint64_t current_ = 0;
    std::string label_;
    std::chrono::steady_clock::time_point start_;
    static constexpr int kWidth = 36;
public:
    ProgressBar(std::string_view label, uint64_t total)
        : total_(total), label_(label), start_(std::chrono::steady_clock::now()) {
        Print();
    }
    void Update(uint64_t n) { current_ = n; Print(); }
    void Tick() { if ((++current_ & 0x1FF) == 0) Print(); }
    void Finish() { current_ = total_; Print(); putchar('\n'); }

private:
    void Print() const {
        double pct = total_ > 0 ? double(current_) / double(total_) : 0.0;
        int filled = int(kWidth * pct);
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        double eta = (pct > 0.001 && pct < 0.999) ? elapsed / pct * (1.0 - pct) : 0.0;

        fmt::print("\r  {:<20s} [", label_);
        for (int i = 0; i < kWidth; i++) putchar(i < filled ? '#' : ' ');
        fmt::print("] {:5.1f}%  {}/{}", pct * 100.0, current_, total_);
        if (eta > 0.5) fmt::print("  ETA {:.0f}s", eta);
        fmt::print("  {:.1f}s", elapsed);
        fflush(stdout);
    }
};
