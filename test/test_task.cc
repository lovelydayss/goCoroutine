
#include "gocoroutine/task.h"
#include "gocoroutine/utils.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

using namespace gocoroutine;

Task<int> simple_task2() {
	DEBUGFMTLOG("task 2 start ...");
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1s);
	DEBUGFMTLOG("task 2 returns after 1s.");
	co_return 2;
}

Task<int> simple_task3() {
	DEBUGFMTLOG("in task 3 start ...");
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(2s);
	DEBUGFMTLOG("task 3 returns after 2s.");
	co_return 3;
}

Task<int> simple_task() {
	DEBUGFMTLOG("task start ...");
	auto result2 = co_await simple_task2();
	DEBUGFMTLOG("returns from task2: ", result2);
	auto result3 = co_await simple_task3();
	DEBUGFMTLOG("returns from task3: ", result3);
	co_return 1 + result2 + result3;
}

TEST_CASE("Task") {
	auto simpleTask = simple_task();
	simpleTask.then([](int i) { DEBUGFMTLOG("simple task end: ", i); })
	    .catching([](std::exception& e) { DEBUGFMTLOG("error occurred", e.what()); });
	try {
		auto i = simpleTask.get_result();
		debug("simple task end from get: ", i);
	} catch (std::exception& e) {
		DEBUGFMTLOG("error: ", e.what());
	}
}
