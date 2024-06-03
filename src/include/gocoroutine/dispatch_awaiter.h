#ifndef GOCOROUTINE_DISPATCH_AWAITER_H
#define GOCOROUTINE_DISPATCH_AWAITER_H

#include "gocoroutine/executor.h"
#include "gocoroutine/utils.h"

GOCOROUTINE_NAMESPACE_BEGIN

// 执行调度器的绑定
class DispatchAwaiter {

public:
	explicit DispatchAwaiter(AbstractExecutor* executor) noexcept
	    : executor_(executor) {}

	constexpr bool await_ready() const { return false; } /* NOLINT */

	void await_suspend(std::coroutine_handle<> handle) const {

		// 协程调度到对应的调度器上
		executor_->execute([handle]() { 
			handle.resume(); });
	}

	void await_resume() {}

private:
	AbstractExecutor* executor_{};
    
};

GOCOROUTINE_NAMESPACE_END

#endif