#ifndef GOCOROUTINE_CHANNEL_AWAITER_H
#define GOCOROUTINE_CHANNEL_AWAITER_H

#include "gocoroutine/executor.h"
#include "gocoroutine/utils.h"
#include <coroutine>
#include <utility>

GOCOROUTINE_NAMESPACE_BEGIN

template <typename T> class Channel;

template <typename T> class WriterAwaiter {

public:
	WriterAwaiter(Channel<T>* channel, T value)
	    : channel_(channel)
	    , value_(value) {}

	WriterAwaiter(WriterAwaiter&& other) noexcept
	    : channel_(std::exchange(other.channel_, {}))
	    , executor_(std::exchange(other.executor_, {}))
	    , value_(other.value_)
	    , handle_(other.handle_) {}

	~WriterAwaiter() {

		if (channel_)
			channel_->remove_writer(this);
	}

public:
	constexpr bool await_ready() { return false; }

	void await_suspend(std::coroutine_handle<> handle) noexcept {

		this->handle_ = handle;
		channel_->try_push_writer(this);
	}

	void await_resume() {
		channel_->check_closed();
		channel_ = nullptr;
	}

	void resume() {
		if (executor_) {
			executor_->execute([this]() { handle_.resume(); });
		} else {
			handle_.resume();
		}
	}

public:
	Channel<T>* channel_{};
	AbstractExecutor* executor_{};
	T value_{};
	std::coroutine_handle<> handle_{};
};

template <typename T> class ReaderAwaiter {
public:
	explicit ReaderAwaiter(Channel<T>* channel)
	    : channel_(channel) {}

	ReaderAwaiter(ReaderAwaiter&& other) noexcept
	    : channel_(std::exchange(other.channel_, {}))
	    , executor_(std::exchange(other.executor_, {}))
	    , value_(other.value_)
	    , p_value_(std::exchange(other.p_value_, {}))
	    , handle_(other.handle_) {}

	~ReaderAwaiter() {

		if (channel_)
			channel_->remove_reader(this);
	}

public:
	bool await_ready() { return false; }

	auto await_suspend(std::coroutine_handle<> handle) {
		this->handle_ = handle;
		channel_->try_push_reader(this);
	}

	int await_resume() {
		auto channel = this->channel_;
		this->channel_ = nullptr;
		channel->check_closed();
		return value_;
	}

    void resume(T value) {
        this->value_ = value;
        if(p_value_) {
            *p_value_ = value;
        }
    }

    void resume() {
        if(executor_) {
            executor_->execute([this]() { handle_.resume(); });
        } else {
            handle_.resume();
        }
    }

public:
	Channel<T>* channel_{};
	AbstractExecutor* executor_{};
	T value_{};
	T* p_value_{};
	std::coroutine_handle<> handle_{};
};

GOCOROUTINE_NAMESPACE_END

#endif