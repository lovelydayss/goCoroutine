#ifndef GOCOROUTINE_TASK_AWAITER_H
#define GOCOROUTINE_TASK_AWAITER_H

#include "gocoroutine/executor.h"
#include "gocoroutine/utils.h"
#include <utility>

GOCOROUTINE_NAMESPACE_BEGIN

template <typename ResultType, typename Executor> class Task;

template <typename Result, typename Executor> class TaskAwaiter {

public:
	explicit TaskAwaiter(AbstractExecutor* executor,
	                     Task<Result, Executor>&& task) noexcept
	    : executor_(executor)
	    , task_(std::move(task)) {}
	TaskAwaiter(TaskAwaiter&& completion) noexcept
	    : executor_(completion.executor_)
	    , task_(std::exchange(completion.task_, {})) {}

	TaskAwaiter(TaskAwaiter& value) = delete;
	TaskAwaiter& operator=(TaskAwaiter&) = delete;

public:
	constexpr bool await_ready() const noexcept { /* NOLINT */
		return false;
	}

	// handle 表示当前协程
	// 可以通过 handle.resume() 来唤醒当前协程，从而继续执行
	void await_suspend(std::coroutine_handle<> handle) noexcept {
		task_.finally([handle]() { handle.resume(); });
	}

	Result await_resume() noexcept { return task_.get_result(); }

private:
	Task<Result, Executor> task_{};
	AbstractExecutor* executor_{};
};

GOCOROUTINE_NAMESPACE_END

#endif
