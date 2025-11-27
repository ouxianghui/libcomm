//
//  task_scheduler.hpp
//  PeerClient
//
//  Created by Jackie Ou on 2025/6/23.
//  Copyright © 2025 RingCentral. All rights reserved.
//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <chrono>
#include "absl/functional/any_invocable.h"
#include "api/function_view.h"
#include "api/location.h"
#include "api/units/time_delta.h"
#include "rtc_base/thread.h"

namespace base {

/**
 * @brief Thread management class for managing multiple rtc::Thread instances
 * 
 * Provides a centralized way to create, manage, and dispatch tasks to different threads.
 * Each thread has a unique name and can be used for specific purposes.
 * 
 * This class is implemented as a singleton to ensure global thread management.
 */
class TaskScheduler {
public:
    /**
     * @brief Get the singleton instance
     * @return Pointer to the singleton instance
     */
    static TaskScheduler* instance();

    /**
     * @brief Start the singleton instance
     */
    static void startup();

    /**
     * @brief Stop the singleton instance
     */
    static void shutdown();

    ~TaskScheduler();

    /**
     * @brief Create a new thread with the given name
     * @param name Thread name for identification
     * @return Pointer to the created thread, or nullptr if creation failed
     */
    webrtc::Thread* createThread(const std::string& name);

    /**
     * @brief Create multiple threads from a list of thread names
     * @param threadNames Vector of thread names to create
     * @return Vector of created thread pointers (nullptr for failed creations)
     */
    std::vector<webrtc::Thread*> createThreads(const std::vector<std::string>& threadNames);

    /**
     * @brief Get a thread by name
     * @param name Thread name
     * @return Pointer to the thread, or nullptr if not found
     */
    webrtc::Thread* getThread(const std::string& name) const;

    /**
     * @brief Get the main thread
     * @return Pointer to the main thread
     */
    webrtc::Thread* getMainThread() const;

    /**
     * @brief Dispatch a task to a specific thread
     * @param threadName Name of the target thread
     * @param task Task to execute
     * @param location Location information for debugging/tracking
     */
    void dispatch(const std::string& threadName, 
                  absl::AnyInvocable<void() &&> task,
                  const webrtc::Location& location = webrtc::Location::Current());

    /**
     * @brief Dispatch a task to a specific thread after a delay
     * @param threadName Name of the target thread
     * @param task Task to execute
     * @param delay Delay before execution
     * @param location Location information for debugging/tracking
     */
    void dispatchAfter(const std::string& threadName,
                       absl::AnyInvocable<void() &&> task,
                       webrtc::TimeDelta delay,
                       const webrtc::Location& location = webrtc::Location::Current());

    /**
     * @brief Dispatch a task to the main thread
     * @param task Task to execute
     * @param location Location information for debugging/tracking
     */
    void dispatchToMain(absl::AnyInvocable<void() &&> task,
                        const webrtc::Location& location = webrtc::Location::Current());

    /**
     * @brief Dispatch a task to the main thread after a delay
     * @param task Task to execute
     * @param delay Delay before execution
     * @param location Location information for debugging/tracking
     */
    void dispatchToMainAfter(absl::AnyInvocable<void() &&> task,
                             webrtc::TimeDelta delay,
                             const webrtc::Location& location = webrtc::Location::Current());

    /**
     * @brief Blocking call to a specific thread
     * @param threadName Name of the target thread
     * @param functor Function to execute
     * @param location Location information for debugging/tracking
     */
    void blockingCall(const std::string& threadName,
                      webrtc::FunctionView<void()> functor,
                      const webrtc::Location& location = webrtc::Location::Current());

    /**
     * @brief Blocking call to the main thread
     * @param functor Function to execute
     * @param location Location information for debugging/tracking
     */
    void blockingCallToMain(webrtc::FunctionView<void()> functor,
                            const webrtc::Location& location = webrtc::Location::Current());

    /**
     * @brief Check if current thread is the main thread
     * @return true if current thread is main thread
     */
    bool isMainThread() const;

    /**
     * @brief Check if current thread is a specific thread
     * @param threadName Name of the thread to check
     * @return true if current thread matches the specified thread
     */
    bool isCurrentThread(const std::string& threadName) const;

    /**
     * @brief Get the name of the current thread
     * @return Name of the current thread, or empty string if not managed
     */
    std::string getCurrentThreadName() const;

    /**
     * @brief Start all managed threads
     */
    void start();

    /**
     * @brief Stop all managed threads
     */
    void stop();

    /**
     * @brief Check if the scheduler is running
     * @return true if scheduler is running
     */
    bool isRunning() const;

    /**
     * @brief Get all thread names
     * @return Vector of thread names
     */
    std::vector<std::string> getThreadNames() const;

    /**
     * @brief Remove a thread from management
     * @param name Name of the thread to remove
     * @return true if thread was found and removed
     */
    bool removeThread(const std::string& name);
    
    /**
     * @brief Process messages on the main thread
     * @param timeout Timeout in milliseconds
     */
    void processMessages(int timeout = 50);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    TaskScheduler();

    /**
     * @brief Initialize the main thread
     * @return true if initialization successful
     */
    bool initializeMainThread();

    /**
     * @brief Find thread by name (thread-safe)
     * @param name Thread name
     * @return Pointer to thread, or nullptr if not found
     */
    webrtc::Thread* findThread(const std::string& name) const;

    /**
     * @brief Get current thread name (thread-safe)
     * @return Current thread name
     */
    std::string getCurrentThreadNameInternal() const;

    /**
     * @brief Cleanup the singleton instance
     */
    void cleanup();
    
    /**
     * @brief Internal cleanup method
     */
    void cleanupInternal();
    
    // Disable copy and move
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<webrtc::Thread>> m_threads;
    webrtc::Thread* m_mainThread = nullptr;
#if defined(WEBRTC_LINUX)
    thread_local static pthread_t m_threadId;
#elif defined(WIN32)
    DWORD m_threadId;
#endif
    std::atomic<bool> m_running{false};
    
    // Thread-local storage for current thread name
    static thread_local std::string s_currentThreadName;
};

} // namespace rcvpc 
