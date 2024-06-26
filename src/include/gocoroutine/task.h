#ifndef GOCOROUTINE_TASK_H
#define GOCOROUTINE_TASK_H

#include "gocoroutine/dispatch_awaiter.h"
#include "gocoroutine/executor.h"
#include "gocoroutine/result.h"
#include "gocoroutine/sleep_awaiter.h"
#include "gocoroutine/task_awaiter.h"
#include "gocoroutine/channel_awaiter.h"
#include "gocoroutine/utils.h"
#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

GOCOROUTINE_NAMESPACE_BEGIN

// 此处为通用协程任务定义
// 在使用中使用 Task 包裹返回值，内部包含 co_return 即可
// 此后即可在外部函数中对该协程进行切换等操作
// 参考 https://zhuanlan.zhihu.com/p/615828280

template <typename ResultType, typename Executor = NewThreadExecutor>
class Task {

public:
	// promise 类型定义
	class TaskPromise {
	public:
		// 协程初始化及结束后操作内容
		DispatchAwaiter initial_suspend() noexcept {
			return DispatchAwaiter{&executor_};
		}
		
		// 系统默认实现了 suspend_always 和 suspend_never 两个 awaiter
		// 仅区别在 await_ready() 函数的返回值
		// 这里 suspend_always 标识返回 false 情况，表示持续中断
		// 可以使用 suspend_never 标识返回 true 情况，表示跳过中断                                                          
		std::suspend_always final_suspend() noexcept { return {}; } /* NOLINT */

		// 构造协程返回对象
		Task<ResultType, Executor> get_return_object() {
			return Task{
			    std::coroutine_handle<TaskPromise>::from_promise(*this)};
		}

		// await 转换函数，用于对 await 传入参数进行处理
		template <typename ResultType_, typename Executor_>
		TaskAwaiter<ResultType_, Executor_>
		await_transform(Task<ResultType_, Executor_>&& task) {
			return TaskAwaiter<ResultType_, Executor_>(&executor_,
			                                           std::move(task));
		}

		// await 转换函数，用于解决延时调用问题
		template <typename Rep_, typename Period_>
		SleepAwaiter
		await_transform(std::chrono::duration<Rep_, Period_>&& duration) {
			return SleepAwaiter(
			    &executor_,
			    std::chrono::duration_cast<std::chrono::milliseconds>(duration)
			        .count());
		}

		// await 转换函数，用于处理读 channel
		template <typename ValueType_>
		auto await_transform(ReaderAwaiter<ValueType_> reader_awaiter) {
			reader_awaiter.executor_ = &executor_;
			return reader_awaiter;
		}

		// await 转换函数，用于处理写 channel
		template <typename ValueType_>
		auto await_transform(WriterAwaiter<ValueType_> writer_awaiter) {
			writer_awaiter.executor_ = &executor_;
			return writer_awaiter;
		}

		// co_return 调用返回值，对于 void 类型特例化为 return_void
		void return_value(ResultType value) {
			std::unique_lock<std::mutex> lock(completion_mutex_);
			result_ = Result<ResultType>(std::move(value));
			completion_.notify_all();
			lock.unlock();

			notify_callbacks();
		}

		// 异常处理
		void unhandled_exception() {
			std::unique_lock<std::mutex> lock(completion_mutex_);
			result_ = Result<ResultType>(std::current_exception());
			completion_.notify_all();
			lock.unlock();

			notify_callbacks();
		}

		// 同步获取回调值
		ResultType get_result() {
			std::unique_lock<std::mutex> lock(completion_mutex_);

			if (!result_.has_value())
				completion_.wait(lock); // 当前线程阻塞同时释放锁

			return result_->get_or_throw();
		}

		// 异步获取回调值
		void on_completed(std::function<void(Result<ResultType>)>&& func) {
			std::unique_lock<std::mutex> lock(completion_mutex_);

			if (result_.has_value()) {
				auto value = result_.value();
				lock.unlock();
				func(value);
				return;
			} else {
				callbacks_.push_back(std::move(func));
			}
		}

	private:
		void notify_callbacks() {
			std::unique_lock<std::mutex> lock(completion_mutex_);
			auto value = result_.value();
			auto callbacks = std::exchange(callbacks_, {});
			lock.unlock();

			for (auto& callback : callbacks) {
				callback(value);
			}
		}

	private:
		std::optional<Result<ResultType>> result_{};
		std::list<std::function<void(Result<ResultType>)>> callbacks_{};

		Executor executor_{};

		std::mutex completion_mutex_{};
		std::condition_variable completion_{};
	};

public:
	// 满足 c++20 coroutines 协程中对 promise_type 类型定义的要求
	using promise_type = Task<ResultType, Executor>::TaskPromise;

public:
	explicit Task(std::coroutine_handle<promise_type> handle)
	    : handle_(handle) {}

	Task(Task&& task) noexcept
	    : handle_(std::exchange(task.handle_, {})) {}

	Task(Task& value) = delete;
	Task& operator=(Task& value) = delete;

	~Task() {
		if (handle_)
			handle_.destroy();
	}

public:
	
	// 在这里均是通过 promise_type 类型来将协程操作转换为对应 promise_type 内部操作
	ResultType get_result() { return handle_.promise().get_result(); }

	Task& then(std::function<void(ResultType)>&& func) {
		handle_.promise().on_completed(
		    [func](auto result) { // Result<ResultType>
			    try {
				    auto res = result.get_or_throw();
				    func(res);
			    } catch (std::exception& e) {
				    // ignore
			    }
		    });

		return *this;
	}

	Task& catching(std::function<void(std::exception&)>&& func) {

		handle_.promise().on_completed(
		    [func](auto result) { // Result<ResultType>
			    try {
				    result.get_or_throw();
			    } catch (std::exception& e) {
				    func(e);
			    }
		    });

		return *this;
	}

	// 这里似乎没有对 finally 和 then 做显式区分
	Task& finally(std::function<void()>&& func) {
		handle_.promise().on_completed([func](auto result) { func(); });
		return *this;
	}

private:

	// 协程句柄
	// 可实现数据中转的功能，promise() 函数即可调用内部 prosie_type 对象
	std::coroutine_handle<promise_type> handle_{};
};

// void 特化版本
template <typename Executor> class Task<void, Executor> {
public:
	// promise 类型定义
	class TaskPromise {
	public:
		DispatchAwaiter initial_suspend() noexcept {
			return DispatchAwaiter{&executor_};
		}                                                           /* NOLINT */
		std::suspend_always final_suspend() noexcept { return {}; } /* NOLINT */

		Task<void, Executor> get_return_object() {
			return Task{
			    std::coroutine_handle<TaskPromise>::from_promise(*this)};
		}

		template <typename ResultType_, typename Executor_>
		TaskAwaiter<ResultType_, Executor_>
		await_transform(Task<ResultType_, Executor_>&& task) {
			return TaskAwaiter<ResultType_, Executor_>{std::move(task)};
		}

		// await 转换函数，用于解决延时调用问题
		template <typename Rep_, typename Period_>
		SleepAwaiter
		await_transform(std::chrono::duration<Rep_, Period_>&& duration) {
			return SleepAwaiter(
			    &executor_,
			    std::chrono::duration_cast<std::chrono::milliseconds>(duration)
			        .count());
		}

		// await 转换函数，用于处理读 channel
		template <typename ValueType_>
		auto await_transform(ReaderAwaiter<ValueType_> reader_awaiter) {
			reader_awaiter.executor_ = &executor_;
			return reader_awaiter;
		}

		// await 转换函数，用于处理写 channel
		template <typename ValueType_>
		auto await_transform(WriterAwaiter<ValueType_> writer_awaiter) {
			writer_awaiter.executor_ = &executor_;
			return writer_awaiter;
		}

		void return_void() {
			std::unique_lock<std::mutex> lock(completion_mutex_);
			result_ = Result<void>();
			completion_.notify_all();
			lock.unlock();

			notify_callbacks();
		}

		void unhandled_exception() {
			std::unique_lock<std::mutex> lock(completion_mutex_);
			result_ = Result<void>(std::current_exception());
			completion_.notify_all();
			lock.unlock();

			notify_callbacks();
		}

		void get_result() {
			std::unique_lock<std::mutex> lock(completion_mutex_);

			if (!result_.has_value())
				completion_.wait(lock);

			result_->get_or_throw();
		}

		void on_completed(std::function<void(Result<void>)>&& func) {
			std::unique_lock<std::mutex> lock(completion_mutex_);

			if (result_.has_value()) {
				auto value = result_.value();
				lock.unlock();
				func(value);
				return;
			} else {
				callbacks_.push_back(std::move(func));
			}
		}

	private:
		void notify_callbacks() {
			std::unique_lock<std::mutex> lock(completion_mutex_);
			auto value = result_.value();
			auto callbacks = std::exchange(callbacks_, {});
			lock.unlock();

			for (auto& callback : callbacks) {
				callback(value);
			}
		}

	private:
		std::optional<Result<void>> result_{};
		std::list<std::function<void(Result<void>)>> callbacks_{};

		Executor executor_{};

		std::mutex completion_mutex_{};
		std::condition_variable completion_{};
	};

public:
	using promise_type = Task<void, Executor>::TaskPromise;

public:
	explicit Task(std::coroutine_handle<promise_type> handle)
	    : handle_(handle) {}

	Task(Task&& task) noexcept
	    : handle_(std::exchange(task.handle_, {})) {}

	Task(Task& value) = delete;
	Task& operator=(Task& value) = delete;

	~Task() {
		if (handle_)
			handle_.destroy();
	}

public:
	void get_result() { handle_.promise().get_result(); }

	Task& then(std::function<void()>&& func) {
		handle_.promise().on_completed(
		    [func](auto result) { // Result<ResultType>
			    try {
				    result.get_or_throw();
				    func();
			    } catch (std::exception& e) {
				    // ignore
			    }
		    });

		return *this;
	}

	Task& catching(std::function<void(std::exception&)>&& func) {

		handle_.promise().on_completed([func](auto result) {
			try {
				result.get_or_throw();
			} catch (std::exception& e) {
				func(e);
			}
		});

		return *this;
	}

	Task& finally(std::function<void()>&& func) {
		handle_.promise().on_completed([func](auto result) { func(); });
		return *this;
	}

private:
	std::coroutine_handle<promise_type> handle_{};
};

GOCOROUTINE_NAMESPACE_END

#endif