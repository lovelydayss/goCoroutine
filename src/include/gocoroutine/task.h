#ifndef GOCOROUTINE_TASK_H
#define GOCOROUTINE_TASK_H

#include "gocoroutine/task_promise.h"
#include "gocoroutine/utils.h"
#include <exception>
#include <functional>
#include <utility>

GOCOROUTINE_NAMESPACE_BEGIN

template <typename ResultType> class Task {

public:
	using promise_type = TaskPromise<ResultType>;

public:
	explicit Task(std::coroutine_handle<promise_type> handle)
	    : m_handle(handle) {}

	Task(Task&& task) noexcept
	    : m_handle(std::exchange(task.handle, {})) {}

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
		    [func](auto result) {       // Result<ResultType>
			    try {
				    func(result.get_or_throw());

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
		m_handle.promise().on_complete([func](auto result) { func(); });
		return *this;
	}

private:
	std::coroutine_handle<promise_type> m_handle;
};

GOCOROUTINE_NAMESPACE_END

#endif