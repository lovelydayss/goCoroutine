#ifndef GOCOROUTINE_SCHEDULER_H
#define GOCOROUTINE_SCHEDULER_H

#include "gocoroutine/utils.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

GOCOROUTINE_NAMESPACE_BEGIN

// 此处设计为定时事件（即将任务与时间戳进行绑定，从而实现一个毫秒级别的延时）
// 采用 operator() 重载，实现对于任务的调用
class DelayedExecutable {

public:
	// 构造函数，将任务与时间戳进行绑定
	DelayedExecutable(std::function<void()>&& func, long long delay)
	    : func_(func) {
		auto now = std::chrono::system_clock::now();
		auto current = std::chrono::duration_cast<std::chrono::milliseconds>(
		                   now.time_since_epoch())
		                   .count();
		scheduler_time_ = current + delay;
	}

public:
	// 延时函数，计算剩余时间
	int64_t delay() const { /* NOLINT */
		auto now = std::chrono::system_clock::now();
		auto current = std::chrono::duration_cast<std::chrono::milliseconds>(
		                   now.time_since_epoch())
		                   .count();
		return scheduler_time_ - current;
	}

	// 返回执行时间戳
	int64_t get_scheduled_time() const { /* NOLINT */
		return scheduler_time_;
	}

	// operator() 重载，实现对于定时任务的调用
	void operator()() { func_(); }

private:
	int64_t scheduler_time_{};     // 调度时间戳
	std::function<void()> func_{}; // 延时执行函数
};

// DelayedExecutableCompare 类，用于实现 DelayedExecutable 类的比较
// 定义了 operator() 重载，用于比较 DelayedExecutable 类的调度时间戳
// 调度时间戳越小的，优先级越高
class DelayedExecutableCompare {
public:
	bool operator()(DelayedExecutable& left, DelayedExecutable& right) {
		return left.get_scheduled_time() > right.get_scheduled_time();
	}
};

// 这里相当于一个定时调度器的实现
// executor LoopExexutor
// 的基础上，使用优先级队列（通过重载时间戳比较函数）实现按剩余等待时间的排序
// 使用条件变量 condition_variable 实现阻塞至等待时间位置
class Scheduler {

public:
	Scheduler() {
		is_active_.store(true, std::memory_order_relaxed);
		work_thread_ = std::thread(&Scheduler::run_loop, this);
	}

	~Scheduler() {
		shutdown();
		join();
	}

public:
	// 此处实现基本与 LoopExexutor 相同，此处省略注释
	void execute(std::function<void()>&& func, int64_t delay) {
		delay = delay < 0 ? 0 : delay;
		std::unique_lock<std::mutex> lock(queue_mutex_);

		if (is_active_.load(std::memory_order_relaxed)) {
			bool need_notify = executable_queue_.empty() ||
			                   executable_queue_.top().delay() > delay;
			executable_queue_.emplace(std::move(func), delay);
			lock.unlock();

			if (need_notify) {
				queue_condition_.notify_all();
			}
		}
	}

	void shutdown(bool wait_for_complete = true) {

		if (!is_active_.load(std::memory_order_relaxed))
			return;

		is_active_.store(false, std::memory_order_relaxed);

		if (!wait_for_complete) {

			// 清空任务队列
			std::unique_lock<std::mutex> lock(queue_mutex_);
			std::exchange(executable_queue_, {});
			lock.unlock();

			// 唤醒所有阻塞线程
			queue_condition_.notify_all();
		}
	}

	void join() {
		if (work_thread_.joinable()) {
			work_thread_.join();
		}
	}

private:
	// 此处 Scheduler 与 LoopExecutor 区别主要体现在此处
	// 增加基于优先级队列及条件变量实现的定时逻辑
	void run_loop() {
		while (is_active_.load(std::memory_order_relaxed) ||
		       !executable_queue_.empty()) {

			std::unique_lock<std::mutex> lock(queue_mutex_);

			// 当任务队列为空时，执行阻塞
			if (executable_queue_.empty()) {
				queue_condition_.wait(lock);
				if (executable_queue_.empty()) {
					continue;
				}
			}

			// 获取当前最近任务
			auto executable = executable_queue_.top();
			int64_t delay = executable.delay();

			// 未到执行时间
			if (delay > 0) {

				// 按当前最近时间进行阻塞，直到到执行时间
				auto status = queue_condition_.wait_for(
				    lock, std::chrono::milliseconds(delay));

				// 判断唤醒状态
				// 当为超时，说明到最近任务执行时间了，直接向下执行至后续调用
				// func 逻辑 当不为超时，说明有其他任务到来唤醒，因此直接
				// continue 重新执行逻辑比较等
				if (status != std::cv_status::timeout) {
					continue;
				}
			}

			// 执行任务
			executable_queue_.pop();
			lock.unlock();

			executable();
		}

		// DEBUGFMTLOG("timer run loop exit!");
	}

private:
	std::condition_variable queue_condition_{};
	std::mutex queue_mutex_{};
	std::priority_queue<DelayedExecutable, std::vector<DelayedExecutable>,
	                    DelayedExecutableCompare>
	    executable_queue_{};

	std::atomic<bool> is_active_{};
	std::thread work_thread_{};
};

GOCOROUTINE_NAMESPACE_END

#endif