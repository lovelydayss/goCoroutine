# if 0
#ifndef GOCOROUTINE_TASK_PROMISE_H
#define GOCOROUTINE_TASK_PROMISE_H

#include "gocoroutine/result.h"
#include "gocoroutine/utils.h"
#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <mutex>
#include <optional>

GOCOROUTINE_NAMESPACE_BEGIN


template <typename ResultType> class Task;
template <> class Task<void>;

template <typename ResultType> class TaskPromise {
public:
	std::suspend_never initial_suspend() noexcept { return {}; }

	std::suspend_always final_suspend() noexcept { return {}; }

	Task<ResultType> get_return_object() {
		return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
	}

	template <typename ResultType_> 
	TaskAwaiter<ResultType_> await_transform(Task<ResultType_>&& task) {
		return TaskAwaiter<ResultType_>{std::move(task)};
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

template <> class TaskPromise<void> {
public:
	std::suspend_never initial_suspend() noexcept { return {}; }

	std::suspend_always final_suspend() noexcept { return {}; }

	Task<void> get_return_object() {
		return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
	}

	template <typename ResultType_>
	TaskAwaiter<ResultType_> await_transform(Task<ResultType_>&& task) {
		return TaskAwaiter<ResultType_>{std::move(task)};
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

GOCOROUTINE_NAMESPACE_END

#endif

#endif