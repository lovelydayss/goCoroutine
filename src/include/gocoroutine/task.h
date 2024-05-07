#ifndef GOCOROUTINE_TASK_H
#define GOCOROUTINE_TASK_H

#include "gocoroutine/result.h"
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

template <typename ResultType> class Task {

public:
	template <typename Result> class TaskAwaiter {

	public:
		explicit TaskAwaiter(Task<Result>&& task)
		    : m_task(std::move(task)) {}
		TaskAwaiter(TaskAwaiter&& completion) noexcept
		    : m_task(std::exchange(completion.get_task(), {})) {}

		TaskAwaiter(TaskAwaiter& value) = delete;
		TaskAwaiter& operator=(TaskAwaiter&) = delete;

	public:
		Task<Result>& get_task() { return m_task; }

		constexpr bool await_ready() const noexcept { /* NOLINT */
			return false;
		}

		void await_suspend(std::coroutine_handle<> handle) noexcept {

			m_task.finally([handle]() { handle.resume(); });
		}

		Result await_resume() noexcept { return m_task.get_result(); }

	private:
		Task<Result> m_task;
	};

	class TaskPromise {
	public:
		std::suspend_never initial_suspend() noexcept { return {}; }

		std::suspend_always final_suspend() noexcept { return {}; }

		Task<ResultType> get_return_object() {
			return Task{
			    std::coroutine_handle<TaskPromise>::from_promise(*this)};
		}

		template <typename _ResultType> /* NOLINT */
		TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
			return TaskAwaiter<_ResultType>{std::move(task)};
		}

		void return_value(ResultType value) {
			std::unique_lock<std::mutex> lock(m_completion_mutex);
			m_result = Result<ResultType>(std::move(value));
			m_completion.notify_all();
			notify_callbacks();
		}

		void unhandled_exception() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);
			m_result = Result<ResultType>(std::current_exception());
			m_completion.notify_all();
			notify_callbacks();
		}

		ResultType get_result() {
			std::unique_lock<std::mutex> lock(m_completion_mutex);

			if (!m_result.has_value())
				m_completion.wait(lock);

			return m_result->get_or_throw();
		}

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
	using promise_type = Task<ResultType>::TaskPromise;

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
template <> class Task<void> {
public:
	template <typename Result> class TaskAwaiter {

	public:
		explicit TaskAwaiter(Task<Result>&& task)
		    : m_task(std::move(task)) {}
		TaskAwaiter(TaskAwaiter&& completion) noexcept
		    : m_task(std::exchange(completion.get_task(), {})) {}

		TaskAwaiter(TaskAwaiter& value) = delete;
		TaskAwaiter& operator=(TaskAwaiter&) = delete;

	public:
		Task<Result>& get_task() { return m_task; }

		constexpr bool await_ready() const noexcept { /* NOLINT */
			return false;
		}

		void await_suspend(std::coroutine_handle<> handle) noexcept {

			m_task.finally([handle]() { handle.resume(); });
		}

		void await_resume() noexcept {
			m_task.get_result();
			DEBUGFMTLOG("VOID RESUME");
		}

	private:
		Task<Result> m_task;
	};

	class TaskPromise {
	public:
	public:
		std::suspend_never initial_suspend() noexcept { return {}; }

		std::suspend_always final_suspend() noexcept { return {}; }

		Task<void> get_return_object() {
			return Task{
			    std::coroutine_handle<TaskPromise>::from_promise(*this)};
		}

		template <typename _ResultType> /* NOLINT */
		TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
			return TaskAwaiter<_ResultType>{std::move(task)};
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
	using promise_type = Task<void>::TaskPromise;

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