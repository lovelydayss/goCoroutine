#ifndef GOCOROUTINE_CHANNEL_H
#define GOCOROUTINE_CHANNEL_H

#include "gocoroutine/channel_awaiter.h"
#include "gocoroutine/utils.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <list>
#include <mutex>
#include <queue>
#include <utility>

GOCOROUTINE_NAMESPACE_BEGIN

template <typename T> class Channel {
public:
	class ChannelClosedException : std::exception {
	public:
		const char* what() const noexcept override { /* NOLINT */
			return "Channel is closed";
		}
	};

public:
	explicit Channel(int capacity = 0)
	    : buffer_capacity_(capacity) {
		is_active_.store(true, std::memory_order_relaxed);
	}

	~Channel() { close(); }

	Channel(const Channel& channel) = delete;
	Channel& operator=(const Channel& cahnnel) = delete;

	Channel(Channel&& cahnnel) = delete;

public:
	bool is_active() { return is_active_.load(std::memory_order_relaxed); }

	void check_closed() {
		if (!is_active_.load(std::memory_order_relaxed)) {
			throw ChannelClosedException();
		}
	}

	WriterAwaiter<T> write(T value) {
		check_closed();
		return WriterAwaiter<T>(this, value);
	}

	ReaderAwaiter<T> read() {
		check_closed();
		return ReaderAwaiter<T>(this);
	}

	WriterAwaiter<T> operator<<(T value) { return write(value); }

	ReaderAwaiter<T> operator>>(T& value_ref) {

		auto awaiter = read();
		awaiter.p_value_ = &value_ref;
		return awaiter;
	}

	void close() {

		bool expect = true;
		if (is_active_.compare_exchange_strong(expect, false,
		                                       std::memory_order_relaxed)) {
			clean_up();
		}
	}

public:
	void try_push_reader(ReaderAwaiter<T>* reader) {
		std::unique_lock<std::mutex> lk(channel_mutex_);
		check_closed();

		if (!buffer_.empty()) {
			auto value = buffer_.front();
			buffer_.pop();

			if (!writer_list_.empty()) {
				auto writer = writer_list_.front();
				writer_list_.pop_front();
				buffer_.push(writer->value_);
				lk.unlock();
				writer->resume();
			} else {
				lk.unlock();
			}

			reader->resume(value);
            return;
		}

        if (!writer_list_.empty()) {
            auto writer = writer_list_.front();
            writer_list_.pop_front();
            lk.unlock();

            reader->resume(writer->value_);
            writer->resume();
            return;
        }

	}

	void try_push_writer(WriterAwaiter<T>* writer) {
        std::unique_lock<std::mutex> lk(channel_mutex_);
        check_closed();

        if(!reader_list_.empty()) {
            auto reader = reader_list_.front();
            reader_list_.pop_front();
            lk.unlock();

            reader->resume(writer->value_);
            writer->resume();
            return;
        }

        if(buffer_.size() < buffer_capacity_) {
            buffer_.push(writer->value_);
            lk.unlock();
            writer->resume();
            return;
        }

        writer_list_.push_back(writer);

    }

	void remove_reader(ReaderAwaiter<T>* reader) {
		std::unique_lock<std::mutex> lk(channel_mutex_);

		auto size = reader_list_.remove(reader);
		DEBUGFMTLOG("remove_reader: size = {}", size);
	}

	void remove_writer(WriterAwaiter<T>* writer) {

		std::unique_lock<std::mutex> lk(channel_mutex_);

		auto size = writer_list_.remove(writer);
		DEBUGFMTLOG("remove_writer: size = {}", size);
	}

private:
	void clean_up() {

		std::unique_lock<std::mutex> lk(channel_mutex_);

		// 释放所有 writer
		for (auto writer : writer_list_) {
			writer->resume();
		}
		writer_list_.clear();

		// 释放所有 reader
		for (auto reader : reader_list_) {
			reader->resume();
		}
		reader_list_.clear();

		std::exchange(buffer_, {});
	}

private:
	int buffer_capacity_{};
	std::queue<T> buffer_{};

	std::list<WriterAwaiter<T>*> writer_list_{};
	std::list<ReaderAwaiter<T>*> reader_list_{};

	std::atomic<bool> is_active_{};
	std::mutex channel_mutex_{};
	std::condition_variable channel_condition_{};
};

GOCOROUTINE_NAMESPACE_END

#endif