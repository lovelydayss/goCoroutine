
#include "gocoroutine/executor.h"
#include "gocoroutine/scheduler.h"
#include "gocoroutine/task.h"
#include "gocoroutine/utils.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

using namespace gocoroutine;

TEST_CASE("fmtlog") {

	SETLOGLEVEL(fmtlog::LogLevel::DBG);
	// fmtlog::setLogLevel(fmtlog::LogLevel::OFF);
	SETLOGHEADER("[{l}] [{YmdHMSe}] [{t}] [{g}] ");

	CREATEPOLLTHREAD(MILLISECONDS(100));

	DEBUGFMTLOG("test of the task begin!");
	CREATEPOLLTHREAD(1000);
}

/*
TEST_CASE("Scheduler") {

    DEBUGFMTLOG("start");

    // 协程中定时任务会自动绑定到全局 scheduler ，与此处不影响
    auto scheduler = Scheduler();

    scheduler.execute([]() { DEBUGFMTLOG("1"); }, 50);
    scheduler.execute([]() { DEBUGFMTLOG("5"); }, 500);
    scheduler.execute([]() { DEBUGFMTLOG("2"); }, 100);
    scheduler.execute([]() { DEBUGFMTLOG("3"); }, 200);
    scheduler.execute([]() { DEBUGFMTLOG("6"); }, 1000);
    scheduler.execute([]() { DEBUGFMTLOG("4"); }, 300);

    scheduler.shutdown();
    scheduler.join();

    DEBUGFMTLOG("end");
}
*/

Task<void, NoopExecuter> simple_task1() {
	DEBUGFMTLOG("in task 1 start ...");
	using namespace std::chrono_literals;
	co_await 1s;
	DEBUGFMTLOG("task 1 returns after 1s.");
	co_return;
}

Task<int, NoopExecuter> simple_task2() {
	DEBUGFMTLOG("task 2 start ...");
	using namespace std::chrono_literals;
	co_await 2s;
	DEBUGFMTLOG("task 2 returns after 2s.");
	co_return 2;
}

Task<int, NoopExecuter> simple_task3() {
	DEBUGFMTLOG("in task 3 start ...");
	using namespace std::chrono_literals;
	co_await 3s;
	DEBUGFMTLOG("task 3 returns after 3s.");
	co_return 3;
}

Task<int, NoopExecuter> simple_task() {
	DEBUGFMTLOG("task start ...");
	co_await simple_task1();
	using namespace std::chrono_literals;
	co_await 100ms;
	DEBUGFMTLOG("after 100ms");

	auto result2 = co_await simple_task2();
	DEBUGFMTLOG("returns from task2: {}", result2);

	co_await 500ms;
	DEBUGFMTLOG("after 500ms");

	auto result3 = co_await simple_task3();
	DEBUGFMTLOG("returns from task3: {}", result3);

	co_return 1 + result2 + result3;
}

TEST_CASE("tasks") {

	auto simpleTask = simple_task();
	simpleTask.then([](int i) { DEBUGFMTLOG("simple task end: {}", i); })
	    .catching([](std::exception& e) {
		    DEBUGFMTLOG("error occurred {}", e.what());
	    });

	try {
		auto i = simpleTask.get_result();
		DEBUGFMTLOG("simple task end from get: {}", i);
	} catch (std::exception& e) {
		DEBUGFMTLOG("error: {}", e.what());
	}
	
}
