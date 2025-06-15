#include "main_thread.hpp"
#include <thread>
#if defined(WEBRTC_MAC)
#include <dispatch/dispatch.h>
#elif defined(WEBRTC_WIN)

#elif defined(WEBRTC_LINUX)

#endif
#include "rtc_base/thread.h"

MainThread* MainThread::Instance()
{
    static MainThread _instance;
    return &_instance;
}

MainThread::~MainThread()
{
    // 析构时不执行清理，由应用程序显式调用 Cleanup
}

// 初始化 UI 线程
bool MainThread::Initialize()
{
#if defined(WEBRTC_MAC)
    // 获取当前线程（主线程）
    thread_ = webrtc::ThreadManager::Instance()->WrapCurrentThread();
    if (!thread_) {
        // 如果当前线程还没有被包装，创建一个新的 Thread 对象
        thread_ = webrtc::Thread::Create().release();
        webrtc::ThreadManager::Instance()->SetCurrentThread(thread_);
        thread_->Start();
    }
#elif defined(WEBRTC_WIN)
    threadId_ = GetCurrentThreadId();
    thread_ = webrtc::ThreadManager::Instance()->WrapCurrentThread();
    if (!thread_) {
        thread_ = webrtc::Thread::Create().release();
        webrtc::ThreadManager::Instance()->SetCurrentThread(thread_);
        thread_->Start();
    }
#elif defined(WEBRTC_LINUX)
    threadId_ = pthread_self();
    thread_ = webrtc::ThreadManager::Instance()->WrapCurrentThread();
    if (!thread_) {
        thread_ = webrtc::Thread::Create().release();
        webrtc::ThreadManager::Instance()->SetCurrentThread(thread_);
        thread_->Start();
    }
#endif

    if (!thread_) {
        return false;
    }
    return true;
}

void MainThread::Loop(int cmsLoop)
{
    if (!thread_) {
        return;
    }

    while (true) {
        ProcessMessages(cmsLoop);
    }
}

void MainThread::ProcessMessages(int cmsLoop)
{
    if (!thread_) {
        return;
    }

    thread_->ProcessMessages(cmsLoop);
}

// 检查当前是否在 UI 线程
bool MainThread::IsMainThread() const
{
#if defined(WEBRTC_MAC)
    return dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL) == dispatch_queue_get_label(dispatch_get_main_queue());
#elif defined(WEBRTC_WIN)
    return GetCurrentThreadId() == threadId_;
#elif defined(WEBRTC_LINUX)
    return pthread_equal(pthread_self(), threadId_);
#endif
}

void MainThread::PostTask(absl::AnyInvocable<void() &&> task, const webrtc::Location& location)
{
    if (!thread_) {
        return;
    }

    if (IsMainThread()) {
        // 如果已经在 UI 线程，直接执行
        std::move(task)();
    } else {
        // 投递到 UI 线程执行
        thread_->PostTask(std::move(task), location);
    }
}

void MainThread::PostDelayedTask(absl::AnyInvocable<void() &&> task, webrtc::TimeDelta delay, const webrtc::Location& location)
{
    if (!thread_) {
        return;
    }

    thread_->PostDelayedTask(std::move(task), delay, location);
}

void MainThread::BlockingCall(webrtc::FunctionView<void()> functor, const webrtc::Location& location)
{
    if (!thread_) {
        return;
    }

    thread_->BlockingCall(std::move(functor), location);
}

// 安全的清理方法
void MainThread::Cleanup()
{
    if (!thread_) {
        return;
    }

    // 确保不在主线程上执行清理
    if (IsMainThread()) {
        // 如果当前在主线程，创建一个新线程来执行清理
        std::thread cleanupThread([this]() {
            InternalCleanup();
        });
        cleanupThread.join();
    } else {
        // 如果不在主线程，直接执行清理
        InternalCleanup();
    }
}

void MainThread::InternalCleanup()
{
    if (!thread_) {
        return;
    }

    // 停止线程但不等待
    thread_->Stop();

    // 不删除线程对象，因为它是主线程
    // 只是将指针置空
    thread_ = nullptr;
}
