#ifndef GOCOROUTINE_SLEEP_AWAITER_H
#define GOCOROUTINE_SLEEP_AWAITER_H

#include "gocoroutine/executor.h"
#include "gocoroutine/scheduler.h"
#include "gocoroutine/utils.h"
#include <cstdint>

GOCOROUTINE_NAMESPACE_BEGIN

class SleepAwaiter {
public:
	SleepAwaiter(AbstractExecutor* executor, int64_t duration)
	    : executor_(executor)
	    , duration_(duration) {}

public:
	constexpr bool await_ready() { return false; }  /* NOLINT */

	void await_suspend(std::coroutine_handle<> handle) {

		static Scheduler scheduler;

		scheduler.execute(
		    [this, handle]() {
			    executor_->execute(
					[handle]() { 
						handle.resume(); });
		    },
		    duration_);
	}

	void await_resume() {}

private:
	AbstractExecutor* executor_{};
	int64_t duration_{};
};

GOCOROUTINE_NAMESPACE_END

#endif