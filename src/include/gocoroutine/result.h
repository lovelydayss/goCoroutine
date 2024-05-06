#ifndef GOCOROUTINE_RESULT_H_
#define GOCOROUTINE_RESULT_H_

#include "gocoroutine/utils.h"
#include <exception>

GOCOROUTINE_NAMESPACE_BEGIN

// 结果类型，成功则返回值，失败则抛出异常
template <typename T>
class Result {
    public:
        explicit Result() = default;
        explicit Result(T&& value) : m_value(value) {}
        explicit Result(std::exception_ptr&& exceptin_ptr) : m_exceptin_ptr(exceptin_ptr) {}

        T get_or_throw() {                      // 实现成功则返回，否则抛出异常
            if (m_exceptin_ptr) {
                std::rethrow_exception(m_exceptin_ptr);
            }
            return m_value;
        }

    private:
        T m_value;                               // 结果值
        std::exception_ptr m_exceptin_ptr;       // 异常指针

};


GOCOROUTINE_NAMESPACE_END


#endif