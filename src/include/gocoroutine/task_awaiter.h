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

	// 判断当前协程是否已经完成
	// true 表示协程已经完成，不再需要中断
	// false 表示协程尚未完成，需要中断
	constexpr bool await_ready() const noexcept { /* NOLINT */
		return false;
	}

	// handle 表示当前协程句柄
	// 可以通过 handle.resume() 来唤醒当前协程，从而继续执行
	// 此处 handle 即为 co_await 所在协程位置
	// 这里也可以设置返回值 bool 来表示是否需要协程暂停
	// 返回 false 不用暂停，true 则暂停，和上述 await_ready() 相反
	void await_suspend(std::coroutine_handle<> handle) noexcept {

		// 为调用协程任务绑定回调，实现在该协程执行最后调用 handle.resume() 来唤醒当前协程
		task_.finally([handle]() { handle.resume(); });
	}

	// co_await 操作符的返回值，会在协程执行完成或者跳过的时候调用
	// 这里即返回任务运算结果
	// 这里会一直阻塞直到调用协程执行完成，然后返回结果
	Result await_resume() noexcept { return task_.get_result(); }

public:
	Task<Result, Executor> task_{};
	AbstractExecutor* executor_{};
};

GOCOROUTINE_NAMESPACE_END

#endif
