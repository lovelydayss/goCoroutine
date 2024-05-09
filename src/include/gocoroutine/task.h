#ifndef GOCOROUTINE_TASK_H
#define GOCOROUTINE_TASK_H

#include "gocoroutine/result.h"
#include "gocoroutine/utils.h"
#include "gocoroutine/task_awaiter.h"
#include "gocoroutine/executor.h"
#include "gocoroutine/scheduler.h"
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
		std::suspend_never initial_suspend() noexcept { return {}; }	 /* NOLINT */
		std::suspend_always final_suspend() noexcept { return {}; } 	 /* NOLINT */

		// 构造协程返回对象
		Task<ResultType, Executor> get_return_object() {
			return Task{
			    std::coroutine_handle<TaskPromise>::from_promise(*this)};
		}

		// await 转换函数，用于对 await 传入参数进行处理
		template <typename ResultType_, typename Executor_>
		TaskAwaiter<ResultType_, Executor_> await_transform(Task<ResultType_, Executor_>&& task) {
			return TaskAwaiter<ResultType_, Executor_>{std::move(task)};
		}

		// （co_return）调用返回值，此处对于 void 类型特例化为 return_void
		void return_value(ResultType value){
			std::unique_lock<std::mutex> lock(m_completion_mutex);
			m_result = Result<ResultType>(std::move(value));
			m_completion.notify_all();
			notify_callbacks();
		}

		// 异常处理
		void unhandled_exception() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);
			m_result = Result<ResultType>(std::current_exception());
			m_completion.notify_all();
			notify_callbacks();
		}

		// 同步获取回调值
		ResultType get_result() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);

			if (!m_result.has_value())
				m_completion.wait(lock);

			return m_result->get_or_throw();
		}

		// 异步获取回调值
		void on_completed(std::function<void(Result<ResultType>)>&& func) {
			std::unique_lock<std::mutex> lock(m_completion_mutex);

			if (m_result.has_value()) {
				auto value = m_result.value();
				lock.unlock();
				func(value);
				return;
			} else {
				m_callbacks.push_back(std::move(func));
			}
		}

	private:
		void notify_callbacks() {
			auto value = m_result.value();
			for (auto& callback : m_callbacks) {
				callback(value);
			}

			m_callbacks.clear();
		}

	private:
		std::optional<Result<ResultType>> m_result;
		std::list<std::function<void(Result<ResultType>)>> m_callbacks;

		std::mutex m_completion_mutex;
		std::condition_variable m_completion;
	};

public:

	// 满足 c++20 coroutines 协程中对 promise_type 类型定义的要求
	using promise_type = Task<ResultType, Executor>::TaskPromise;

public:
	explicit Task(std::coroutine_handle<promise_type> handle)
	    : m_handle(handle) {}

	Task(Task&& task) noexcept
	    : m_handle(std::exchange(task.m_handle, {})) {}

	Task(Task& value) = delete;
	Task& operator=(Task& value) = delete;

	~Task() {
		if (m_handle)
			m_handle.destroy();
	}

public:
	ResultType get_result() { return m_handle.promise().get_result(); }

	Task& then(std::function<void(ResultType)>&& func) {
		m_handle.promise().on_completed(
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

		m_handle.promise().on_completed([func](auto result) {
			try {
				result.get_or_throw();

			} catch (std::exception& e) {
				func(e);
			}
		});

		return *this;
	}

	Task& finally(std::function<void()>&& func) {
		m_handle.promise().on_completed([func](auto result) { func(); });
		return *this;
	}

private:
	std::coroutine_handle<promise_type> m_handle;
};

// void 特化版本
template <typename Executor> class Task<void, Executor> {
public:

	// promise 类型定义
	class TaskPromise {
	public:
	public:
		std::suspend_never initial_suspend() noexcept { return {}; } 	/* NOLINT */
		std::suspend_always final_suspend() noexcept { return {}; } 	/* NOLINT */

		Task<void, Executor> get_return_object() {
			return Task{
			    std::coroutine_handle<TaskPromise>::from_promise(*this)};
		}

		template <typename ResultType_, typename Executor_> 
		TaskAwaiter<ResultType_, Executor_> await_transform(Task<ResultType_, Executor_>&& task) {
			return TaskAwaiter<ResultType_, Executor_>{std::move(task)};
		}

		void return_void() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);
			m_result = Result<void>(std::current_exception());
			m_completion.notify_all();
			notify_callbacks();
		}

		void unhandled_exception() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);
			m_result = Result<void>(std::current_exception());
			m_completion.notify_all();
			notify_callbacks();
		}

		void get_result() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);

			if (!m_result.has_value())
				m_completion.wait(lock);

			m_result->get_or_throw();
		}

		void on_completed(std::function<void(Result<void>)>&& func) {
			std::unique_lock<std::mutex> lock(m_completion_mutex);

			if (m_result.has_value()) {
				auto value = m_result.value();
				lock.unlock();
				func(value);
				return;
			} else {
				m_callbacks.push_back(std::move(func));
			}
		}

	private:
		void notify_callbacks() {
			auto value = m_result.value();
			for (auto& callback : m_callbacks) {
				callback(value);
			}

			m_callbacks.clear();
		}

	private:
		std::optional<Result<void>> m_result;
		std::list<std::function<void(Result<void>)>> m_callbacks;

		std::mutex m_completion_mutex;
		std::condition_variable m_completion;
	};

public:
	using promise_type = Task<void, Executor>::TaskPromise;

public:
	explicit Task(std::coroutine_handle<promise_type> handle)
	    : m_handle(handle) {}

	Task(Task&& task) noexcept
	    : m_handle(std::exchange(task.m_handle, {})) {}

	
	Task(Task& value) = delete;
	Task& operator=(Task& value) = delete;

	~Task() {
		if (m_handle)
			m_handle.destroy();
	}

public:
	void get_result() { m_handle.promise().get_result(); }

	Task& then(std::function<void()>&& func) {
		m_handle.promise().on_completed(
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

		m_handle.promise().on_completed([func](auto result) {
			try {
				result.get_or_throw();
			} catch (std::exception& e) {
				func(e);
			}
		});

		return *this;
	}

	Task& finally(std::function<void()>&& func) {
		m_handle.promise().on_completed([func](auto result) { func(); });
		return *this;
	}

private:
	std::coroutine_handle<promise_type> m_handle;
};

GOCOROUTINE_NAMESPACE_END

#endif