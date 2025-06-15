#pragma once

#include "absl/functional/any_invocable.h"
#include "api/function_view.h"
#include "api/location.h"
#include "api/units/time_delta.h"

namespace webrtc {
    class Thread;
}

class MainThread {
public:
    static MainThread* Instance();

    // 初始化主线程
    bool Initialize();

    // 主线程循环
    void Loop(int cmsLoop);

    void ProcessMessages(int cmsLoop);

    // 检查当前是否在 UI 线程
    bool IsMainThread() const;

    void PostTask(absl::AnyInvocable<void() &&> task,
                  const webrtc::Location& location = webrtc::Location::Current());

    void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                         webrtc::TimeDelta delay,
                         const webrtc::Location& location = webrtc::Location::Current());

    void BlockingCall(webrtc::FunctionView<void()> functor,
                      const webrtc::Location& location = webrtc::Location::Current());

    // 安全的清理方法
    void Cleanup();

private:
    void InternalCleanup();

    ~MainThread();

private:
    MainThread() = default;

    MainThread(const MainThread&) = delete;

    MainThread& operator=(const MainThread&) = delete;

    webrtc::Thread* thread_ = nullptr;
#if defined(WEBRTC_LINUX)
    thread_local static pthread_t threadId_;
#elif defined(WIN32)
    DWORD threadId_;
#endif
};
