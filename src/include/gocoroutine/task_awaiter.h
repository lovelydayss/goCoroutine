#ifndef GOCOROUTINE_TASK_AWAITER_H_
#define GOCOROUTINE_TASK_AWAITER_H_

#include "gocoroutine/utils.h"
#include <utility>

GOCOROUTINE_NAMESPACE_BEGIN

template <typename ResultType, typename Executor> 
class Task;

template<typename Result, typename Executor> 
class TaskAwaiter {

public:

    explicit TaskAwaiter(Task<Result, Executor>&& task) : m_task(std::move(task)) {}
    TaskAwaiter(TaskAwaiter&& completion) noexcept : m_task(std::exchange(completion.get_task(), {})) { }

    TaskAwaiter(TaskAwaiter& value) = delete;
    TaskAwaiter& operator=(TaskAwaiter& ) = delete;

public:
    Task<Result, Executor>& get_task() {
        return m_task;
    }

    constexpr bool await_ready() const noexcept {       /* NOLINT */
        return false;
    }
    
    void await_suspend(std::coroutine_handle<> handle) noexcept {

        m_task.finally([handle]() {
            handle.resume();
        });
    }

    Result await_resume() noexcept {
        return m_task.get_result();
    }

private:
    Task<Result, Executor> m_task;
    
};

GOCOROUTINE_NAMESPACE_END



#endif
