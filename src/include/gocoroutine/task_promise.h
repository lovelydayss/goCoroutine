#ifndef GOCOROUTINE_TASK_PROMISE_H
#define GOCOROUTINE_TASK_PROMISE_H

#include "gocoroutine/result.h"
#include "gocoroutine/task_awaiter.h"
#include "gocoroutine/utils.h"
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <optional>

GOCOROUTINE_NAMESPACE_BEGIN

template <typename ResultType> class Task;

template <typename ResultType> class TaskPromise {
public:
	std::suspend_never initial_suspend() { return {}; }

	std::suspend_always final_suspend() { return {}; }

	Task<ResultType> get_return_object() {
		return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
	}

	template <typename _ResultType>     /* NOLINT */
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

	std::mutex m_completion_mutex;
	std::condition_variable m_completion;

	std::list<std::function<void(Result<ResultType>)>> m_callbacks;
};

GOCOROUTINE_NAMESPACE_END

#endif
