//
//  task_scheduler.cpp
//  PeerClient
//
//  Created by Jackie Ou on 2025/6/23.
//  Copyright © 2025 RingCentral. All rights reserved.
//

#include "task_scheduler.hpp"
#include <iostream>
#include <algorithm>
#include <thread>
#if defined(WEBRTC_MAC)
#include <dispatch/dispatch.h>
#elif defined(WEBRTC_WIN)

#elif defined(WEBRTC_LINUX)

#endif
// #include "utils/log.hpp"

namespace base {

static const std::string TAG("base::TaskScheduler");

// Thread-local storage initialization
thread_local std::string TaskScheduler::s_currentThreadName;

TaskScheduler* TaskScheduler::instance() {
    static std::unique_ptr<TaskScheduler> _instance;
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        _instance.reset(new TaskScheduler());
    });
    return _instance.get();
}

void TaskScheduler::startup() {
    TaskScheduler::instance()->start();
}

void TaskScheduler::shutdown() {
    TaskScheduler::instance()->stop();
}

TaskScheduler::TaskScheduler() {
    //COMMONS_ILOG(TAG, "Created.");
    initializeMainThread();
}

TaskScheduler::~TaskScheduler() {
    stop();
    //COMMONS_ILOG(TAG, "Destroyed.");
}

bool TaskScheduler::initializeMainThread() {
    std::lock_guard<std::mutex> lock(m_mutex);

#if defined(WEBRTC_MAC)
    // Try to wrap current thread as main thread
    m_mainThread = webrtc::ThreadManager::Instance()->WrapCurrentThread();
    if (!m_mainThread) {
        // If current thread is not wrapped yet, create a new Thread object
        m_mainThread = webrtc::Thread::Create().release();
        if (!m_mainThread) {
            //COMMONS_ELOG(TAG, "Failed to create main thread.");
            return false;
        }
        webrtc::ThreadManager::Instance()->SetCurrentThread(m_mainThread);
        m_mainThread->Start();
    }
#elif defined(WEBRTC_WIN)
    m_threadId = GetCurrentThreadId();
    m_mainThread = webrtc::ThreadManager::Instance()->WrapCurrentThread();
    if (!m_mainThread) {
        m_mainThread = webrtc::Thread::Create().release();
        webrtc::ThreadManager::Instance()->SetCurrentThread(m_mainThread);
        m_mainThread->Start();
    }
#elif defined(WEBRTC_LINUX)
    m_threadId = pthread_self();
    m_mainThread = webrtc::ThreadManager::Instance()->WrapCurrentThread();
    if (!m_mainThread) {
        m_mainThread = webrtc::Thread::Create().release();
        webrtc::ThreadManager::Instance()->SetCurrentThread(m_mainThread);
        m_mainThread->Start();
    }
#endif

    
    // Set thread name for main thread
    m_mainThread->SetName("MainThread", m_mainThread);
    
    // Store main thread in our map
    m_threads["MainThread"] = std::unique_ptr<webrtc::Thread>(m_mainThread);
    
    // Set current thread name for main thread
    s_currentThreadName = "MainThread";
    
    //COMMONS_ILOG(TAG, "Main thread initialized: ", (int64_t)m_mainThread);
    return true;
}

webrtc::Thread* TaskScheduler::createThread(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if thread with this name already exists
    if (m_threads.find(name) != m_threads.end()) {
        //COMMONS_ELOG(TAG, "Thread with name '", name, "' already exists.");
        return m_threads[name].get();
    }
    
    // Always create without socket server
    std::unique_ptr<webrtc::Thread> thread = webrtc::Thread::Create();
    if (!thread) {
        //COMMONS_ELOG(TAG, "Failed to create thread '", name, "'.");
        return nullptr;
    }
    thread->SetName(name, thread.get());
    if (m_running) {
        thread->Start();
    }
    webrtc::Thread* threadPtr = thread.get();
    m_threads[name] = std::move(thread);
    //COMMONS_ILOG(TAG, "Created thread: ", name, " (", (int64_t)threadPtr, ")");
    return threadPtr;
}

std::vector<webrtc::Thread*> TaskScheduler::createThreads(const std::vector<std::string>& threadNames) {
    std::vector<webrtc::Thread*> createdThreads;
    createdThreads.reserve(threadNames.size());
    for (const auto& name : threadNames) {
        webrtc::Thread* thread = createThread(name);
        createdThreads.push_back(thread);
    }
    //COMMONS_ILOG(TAG, "Created ", createdThreads.size(), " threads.");
    return createdThreads;
}

webrtc::Thread* TaskScheduler::getThread(const std::string& name) const {
    return findThread(name);
}

webrtc::Thread* TaskScheduler::getMainThread() const {
    return m_mainThread;
}

void TaskScheduler::dispatch(const std::string& threadName, 
                             absl::AnyInvocable<void() &&> task,
                             const webrtc::Location& location) {
    webrtc::Thread* thread = findThread(threadName);
    if (!thread) {
        //COMMONS_ELOG(TAG, "Thread '", threadName, "' not found for dispatch.");
        return;
    }
    
    if (thread->IsCurrent()) {
        // If already on target thread, execute directly
        std::move(task)();
    } else {
        // Post to target thread
        thread->PostTask(std::move(task), location);
    }
}

void TaskScheduler::dispatchAfter(const std::string& threadName,
                                  absl::AnyInvocable<void() &&> task,
                                  webrtc::TimeDelta delay,
                                  const webrtc::Location& location) {
    webrtc::Thread* thread = findThread(threadName);
    if (!thread) {
        //COMMONS_ELOG(TAG, "Thread '", threadName, "' not found for delayed dispatch.");
        return;
    }
    
    thread->PostDelayedTask(std::move(task), delay, location);
}

void TaskScheduler::dispatchToMain(absl::AnyInvocable<void() &&> task,
                                   const webrtc::Location& location) {
    if (!m_mainThread) {
        //COMMONS_ELOG(TAG, "Main thread not available for dispatch.");
        return;
    }
    
    if (isMainThread()) {
        // If already on main thread, execute directly
        std::move(task)();
    } else {
        // Post to main thread
        m_mainThread->PostTask(std::move(task), location);
    }
}

void TaskScheduler::dispatchToMainAfter(absl::AnyInvocable<void() &&> task,
                                        webrtc::TimeDelta delay,
                                        const webrtc::Location& location) {
    if (!m_mainThread) {
        //COMMONS_ELOG(TAG, "Main thread not available for delayed dispatch.");
        return;
    }
    
    m_mainThread->PostDelayedTask(std::move(task), delay, location);
}

void TaskScheduler::blockingCall(const std::string& threadName,
                                 webrtc::FunctionView<void()> functor,
                                 const webrtc::Location& location) {
    webrtc::Thread* thread = findThread(threadName);
    if (!thread) {
        //COMMONS_ELOG(TAG, "Thread '", threadName, "' not found for blocking call.");
        return;
    }
    
    thread->BlockingCall(std::move(functor), location);
}

void TaskScheduler::blockingCallToMain(webrtc::FunctionView<void()> functor,
                                       const webrtc::Location& location) {
    if (!m_mainThread) {
        //COMMONS_ELOG(TAG, "Main thread not available for blocking call.");
        return;
    }
    
    m_mainThread->BlockingCall(std::move(functor), location);
}

bool TaskScheduler::isMainThread() const {
#if defined(WEBRTC_MAC)
    //return m_mainThread && m_mainThread->IsCurrent();
    return dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL) == dispatch_queue_get_label(dispatch_get_main_queue());
#elif defined(WEBRTC_WIN)
    return GetCurrentThreadId() == m_threadId;
#elif defined(WEBRTC_LINUX)
    return pthread_equal(pthread_self(), m_threadId);
#endif
}

bool TaskScheduler::isCurrentThread(const std::string& threadName) const {
    webrtc::Thread* thread = findThread(threadName);
    return thread && thread->IsCurrent();
}

std::string TaskScheduler::getCurrentThreadName() const {
    return getCurrentThreadNameInternal();
}

void TaskScheduler::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    // Start all threads
    for (auto& pair : m_threads) {
        if (!pair.second->RunningForTest()) {
            pair.second->Start();
        }
    }
    
    //COMMONS_ILOG(TAG, "Started all threads.");
}

void TaskScheduler::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Stop all threads except main thread
    for (auto& pair : m_threads) {
        if (pair.first != "MainThread" && pair.second->RunningForTest()) {
            pair.second->Stop();
        }
    }
    
    cleanup();
    
    //COMMONS_ILOG(TAG, "Stopped all threads.");
}

bool TaskScheduler::isRunning() const {
    return m_running;
}

std::vector<std::string> TaskScheduler::getThreadNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_threads.size());
    
    for (const auto& pair : m_threads) {
        names.push_back(pair.first);
    }
    
    return names;
}

bool TaskScheduler::removeThread(const std::string& name) {
    if (name == "MainThread") {
        //COMMONS_ELOG(TAG, "Cannot remove main thread.");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_threads.find(name);
    if (it == m_threads.end()) {
        return false;
    }
    
    // Stop thread if running
    if (it->second->RunningForTest()) {
        it->second->Stop();
    }
    
    // Remove from map
    m_threads.erase(it);
    
    //COMMONS_ILOG(TAG, "Removed thread: ", name);
    return true;
}

webrtc::Thread* TaskScheduler::findThread(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_threads.find(name);
    if (it != m_threads.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

std::string TaskScheduler::getCurrentThreadNameInternal() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if current thread is any of our managed threads
    for (const auto& pair : m_threads) {
        if (pair.second->IsCurrent()) {
            return pair.first;
        }
    }
    
    return "";
}

void TaskScheduler::cleanup() {
    if (!m_mainThread) {
        return;
    }

    // Ensure not executing cleanup on main thread
    if (isMainThread()) {
        // If currently on main thread, create a new thread to perform cleanup
        std::thread cleanupThread([this]() {
            cleanupInternal();
        });
        cleanupThread.join();
    } else {
        // If not on main thread, perform cleanup directly
        cleanupInternal();
    }
}

void TaskScheduler::cleanupInternal()
{
    if (!m_mainThread) {
        return;
    }

    // Stop thread but don't wait
    m_mainThread->Stop();

    // Don't delete thread object because it's the main thread
    // Just set pointer to null
    m_mainThread = nullptr;
}

void TaskScheduler::processMessages(int timeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mainThread) {
        return;
    }
    if (m_mainThread->IsQuitting()) {
        return;
    }
    m_mainThread->ProcessMessages(timeout);
}

} // namespace rcvpc 
