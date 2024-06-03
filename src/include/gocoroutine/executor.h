#ifndef GOCOROUTINE_EXECUTOR_H
#define GOCOROUTINE_EXECUTOR_H

#include "gocoroutine/utils.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>

GOCOROUTINE_NAMESPACE_BEGIN

// 调度器的抽象基类
// 对于此处的调度器，其基本关系是协程和执行线程之间的一种对应关系
// 即如何将协程（类似于函数，也可以称之为用户级线程）绑定到指定系统级线程上去执行，去完成一种对应关系
class AbstractExecutor {
public:
	// 调度函数，实现为纯虚函数
	virtual void execute(std::function<void()>&& func) = 0;
};

// 这里调度器定义为无任何调度，直接执行 func 函数
class NoopExecuter : public AbstractExecutor {
public:
	void execute(std::function<void()>&& func) override { func(); }
};

// 这里调度器定义为使用一个新建独立线程来运行
class NewThreadExecutor : public AbstractExecutor {
public:
	void execute(std::function<void()>&& func) override {
		auto t = std::thread(func);

		if(t.joinable())
			t.join();
	}
};

// 这里调度器定义为采用 std::async 进行任务分配调度
// 因为 std::async 内部维护线程池，可以减少每次创建新线程带来的开销
class AsyncExecutor : public AbstractExecutor {
public:
	void execute(std::function<void()>&& func) override {
		auto future = std::async(func);
	}
};

// 这里调度器定义为采取循环方式调用
// 对一个线程维持一个循环，通过不断将任务放入线程循环中执行从而实现调度
// one loop per thread 思想
class LooperExecutor : public AbstractExecutor {

public:
	// 设置循环执行标志，启动执行线程
	LooperExecutor() {
		is_active_.store(true, std::memory_order_relaxed);
		work_thread_ = std::thread(&LooperExecutor::run_loop, this);
	}

	// 资源清理，最后分离线程直到其执行完毕
	~LooperExecutor() {
		shutdown(false);
		join();
	}

public:
	// 执行调度，即将任务函数 push 至循环队列中等待执行
	void execute(std::function<void()>&& func) override {
		std::unique_lock<std::mutex> lk(queue_mutex_);

		if (is_active_.load(std::memory_order_relaxed)) {
			executable_queue_.push(func);
			lk.unlock();
			queue_condition_.notify_one();
		}
	}

	// 循环关闭及资源清理
	void shutdown(bool wait_for_complete = true) {

		// 判断调度器循环是否关闭
		if(!is_active_.load(std::memory_order_relaxed))
			return;

		is_active_.store(false, std::memory_order_relaxed);
		if (!wait_for_complete) {

			// 直接清空任务队列
			std::unique_lock<std::mutex> lk(queue_mutex_);
			decltype(executable_queue_) empty_queue;
			std::swap(executable_queue_, empty_queue);
		}

		// 这里其实只需要 notify_one() 即可，因为 wait
		// 在条件变量上的只可能是内部线程
		// 出于最后循环关闭及资源释放语义考虑，采用 notify_all()
		// 唤醒全部阻塞线程
		queue_condition_.notify_all();
	}

    void join() {
        if(work_thread_.joinable()) {
            work_thread_.join();
        }
    }

private:
	// 循环执行逻辑
	void run_loop() {

		// 当循环激活或者还存在未被消费掉的事件（使用或保证在执行完所有任务都能够得到执行）
		// 在调度位置设置了当循环未激活时无法再次添加任务
		while (is_active_.load(std::memory_order_relaxed) ||
		       !executable_queue_.empty()) {
			std::unique_lock lk{queue_mutex_};

			// 任务队列为空执行挂起，等待任务添加时唤醒
			if (executable_queue_.empty()) {
				queue_condition_.wait(lk);

				if (executable_queue_.empty())
					continue;
			}

			// 取出任务
			auto func = executable_queue_.front();
			executable_queue_.pop();

			lk.unlock();

			// 任务执行
			func();
		}

		DEBUGFMTLOG("running loop exit!");
		
	}

private:
	std::condition_variable queue_condition_{};
	std::mutex queue_mutex_{};
	std::queue<std::function<void()>> executable_queue_{};

	std::atomic<bool> is_active_{};
	std::thread work_thread_{};
};

// 功能实现与 LooperExecutor 一致，但全局单例
// 所有采用 SharedLoopExector 协程共享同一个调度器，即调度到同一个线程上
class SharedLooperExecutor : public AbstractExecutor {
public:
	void execute(std::function<void()>&& func) override {
		static LooperExecutor share_looper_execuetor;
		share_looper_execuetor.execute(std::move(func));
	}
};

// 参考 Golang 实现协程调度器
// 任务窃取调度器
class GolangExecutor : public AbstractExecutor {

public:
	void execute(std::function<void()>&& func) override {}
};

GOCOROUTINE_NAMESPACE_END

#endif