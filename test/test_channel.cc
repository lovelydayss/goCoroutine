#include "gocoroutine/channel.h"
#include "gocoroutine/task.h"
#include "gocoroutine/utils.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

using namespace gocoroutine;
using namespace std::chrono_literals;

Task<void, LooperExecutor> Producer(Channel<int> &channel) {
  int i = 0;
  while (i < 10) {
    DEBUGFMTLOG("send: {}", i);
    // 或者使用 write 函数：co_await channel.write(i++);
    co_await (channel << i++);
    co_await 300ms;
  }

  channel.close();
  DEBUGFMTLOG("close channel, exit.");
}

Task<void, LooperExecutor> Consumer(Channel<int> &channel) {
  while (channel.is_active()) {
    try {
      // 或者使用 read 函数：auto received = co_await channel.read();
      int received;
      co_await (channel >> received);
      DEBUGFMTLOG("receive: {}", received);
      co_await 2s;
    } catch (std::exception &e) {
      DEBUGFMTLOG("exception: {}", e.what());
    }
  }

  DEBUGFMTLOG("exit.");
}

Task<void, LooperExecutor> Consumer2(Channel<int> &channel) {
  while (channel.is_active()) {
    try {
      auto received = co_await channel.read();
      DEBUGFMTLOG("receive2: {}", received);
      co_await 3s;
    } catch (std::exception &e) {
      DEBUGFMTLOG("exception2: {}", e.what());
    }
  }

  DEBUGFMTLOG("exit.");
}

TEST_CASE("fmtlog") {

	SETLOGLEVEL(fmtlog::LogLevel::DBG);
	// fmtlog::setLogLevel(fmtlog::LogLevel::OFF);
	SETLOGHEADER("[{l}] [{YmdHMSe}] [{t}] [{g}] ");

	DEBUGFMTLOG("test of the task begin!");
	CREATEPOLLTHREAD(100);
}

TEST_CASE("channel") {
  auto channel = Channel<int>(2);
  auto producer = Producer(channel);
  auto consumer = Consumer(channel);
  auto consumer2 = Consumer2(channel);
 
  // 等待协程执行完成再退出
  producer.get_result();
  consumer.get_result();
  consumer2.get_result();

}