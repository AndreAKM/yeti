// Copyright (c) 2014, Dmitry Senin (seninds@gmail.com)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   1. Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// yeti - C++ lightweight threadsafe logging system
// URL: https://github.com/seninds/yeti.git

#ifndef INC_YETI_YETI_H_
#define INC_YETI_YETI_H_

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <yeti/color.h>

namespace yeti {

enum LogLevel {
  LOG_LEVEL_CRITICAL,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_TRACE
};

// user should not see _Shutdown() in yeti namespace
namespace{ void _Shutdown(); }

class Logger {
 public:
  ~Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger* operator &() = delete;

  static Logger& instance() {
    static Logger logger;

    // register _Shutdown() at std::exit()
    static bool is_atexit_registered = false;
    static std::mutex registered_mutex;
    if (!is_atexit_registered) {
      std::lock_guard<std::mutex> lock(registered_mutex);
      if (!is_atexit_registered) {
        std::atexit(_Shutdown);
        is_atexit_registered = true;
      }
    }
    return logger;
  }

  void SetLevel(LogLevel level) noexcept { instance().level_ = level; }
  int GetLevel() const noexcept { return instance().level_; }

  bool GetColored() const noexcept { return instance().is_colored_; }
  void SetColored(bool is_colored) noexcept { is_colored_ = is_colored; }

  void AddMsg(const std::function<void()>& print_func) noexcept {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    queue_.push(print_func);
    cv_.notify_one();
  }

  void Print() {
    // start processing loop
    do {
      std::unique_lock<std::mutex> queue_lock(queue_mutex_);
      cv_.wait_for(queue_lock, std::chrono::milliseconds(1000),
                   [this] { return !this->queue_.empty(); });
      while (!queue_.empty()) {
        queue_.front()();
        queue_.pop();
      }
    } while (!stop_loop_);
  }

  void Shutdown() noexcept {
    // set flag to stop processing loop
    stop_loop_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  Logger() {
    stop_loop_ = false;
    is_colored_ = true;
    level_ = LogLevel::LOG_LEVEL_INFO;
    thread_ = std::thread(&Logger::Print, this);
  }

  std::mutex queue_mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> queue_;
  std::atomic<bool> stop_loop_;
  std::atomic<bool> is_colored_;
  std::atomic<int> level_;
  std::thread thread_;
};

}  // namespace yeti

#include <yeti/yeti-inl.h>
#include <yeti/macro.h>

#endif  // INC_YETI_YETI_H_