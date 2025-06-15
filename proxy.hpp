//
//  rcv_proxy.hpp
//  rcv
//
//  Created by Jackie Ou on 2025/1/17.
//  Copyright © 2025 RingCentral. All rights reserved.
//

#ifndef _PROXY_H_
#define _PROXY_H_

#include <stddef.h>

#include <memory>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include "api/task_queue/task_queue_base.h"
#include "light_weight_semaphore.hpp"

#if !defined(DISABLE_PROXY_TRACE_EVENTS)
    #define DISABLE_PROXY_TRACE_EVENTS
#endif

namespace base {
    namespace details {
        // Class for tracing the lifetime of MethodCall::Marshal.
        class ScopedTrace {
        public:
            explicit ScopedTrace(const char* class_and_method_name);
            ~ScopedTrace();

        private:
            [[maybe_unused]] const char* const class_and_method_name_;
        };
    }  // namespace details

    namespace string_utils_internal {
        template <int NPlus1>
        struct CompileTimeString {
            char string[NPlus1] = {0};
            constexpr CompileTimeString() = default;
            template <int MPlus1>
            explicit constexpr CompileTimeString(const char (&chars)[MPlus1]) {
                char* chars_pointer = string;
                for (auto c : chars)
                    *chars_pointer++ = c;
            }
            template <int MPlus1>
            constexpr auto Concat(CompileTimeString<MPlus1> b) {
                CompileTimeString<NPlus1 + MPlus1 - 1> result;
                char* chars_pointer = result.string;
                for (auto c : string)
                    *chars_pointer++ = c;
                chars_pointer = result.string + NPlus1 - 1;
                for (auto c : b.string)
                    *chars_pointer++ = c;
                result.string[NPlus1 + MPlus1 - 2] = 0;
                return result;
            }
            constexpr operator const char*() { return string; }
        };
    }  // namespace string_utils_internal

// Makes a constexpr CompileTimeString<X> without having to specify X
// explicitly.
template <int N>
constexpr auto MakeCompileTimeString(const char (&a)[N]) {
    return string_utils_internal::CompileTimeString<N>(a);
}

namespace details {
    template <typename R>
    class ReturnType {
    public:
        template <typename C, typename M, typename... Args>
        void invoke(C* c, M m, Args&&... args) {
            r_ = (c->*m)(std::forward<Args>(args)...);
        }

        R get() { return std::move(r_); }

    private:
        R r_{};
    };

    template <>
    class ReturnType<void> {
    public:
        template <typename C, typename M, typename... Args>
        void invoke(C* c, M m, Args&&... args) {
            (c->*m)(std::forward<Args>(args)...);
        }

        void get() {}
    };

    template <typename C, typename R, typename... Args>
    class MethodCall : public std::enable_shared_from_this<MethodCall<C, R, Args...>> {
    public:
        typedef R (C::*Method)(Args...);

        MethodCall(C* c, Method m, Args&&... args)
        : c_(c)
        , m_(m)
        , args_(std::forward_as_tuple(std::forward<Args>(args)...)) {}

        ~MethodCall() {

        }

        R marshal(webrtc::TaskQueueBase* tq) {
            if (tq->IsCurrent()) {
                invoke(std::index_sequence_for<Args...>());
            } else {
                tq->PostTask([wself = this->weak_from_this()]() {
                    if (auto self = wself.lock()) {
                        self->invoke(std::index_sequence_for<Args...>());
                        self->sema_.signal();
                    }
                });
                // Keep waiting, never timeout
                sema_.wait();
            }
            return r_.get();
        }

    private:
        template <size_t... Is>
        void invoke(std::index_sequence<Is...>) {
            r_.invoke(c_, m_, std::move(std::get<Is>(args_))...);
        }

        C* c_;
        Method m_;
        ReturnType<R> r_;
        std::tuple<Args&&...> args_;
        base::LightweightSemaphore sema_;
    };

    template <typename C, typename R, typename... Args>
    class ConstMethodCall : public std::enable_shared_from_this<ConstMethodCall<C, R, Args...>> {
    public:
        typedef R (C::*Method)(Args...) const;

        ConstMethodCall(const C* c, Method m, Args&&... args)
        : c_(c)
        , m_(m)
        , args_(std::forward_as_tuple(std::forward<Args>(args)...)) {}

        ~ConstMethodCall() {
            
        }

        R marshal(webrtc::TaskQueueBase* tq) {
            if (tq->IsCurrent()) {
                invoke(std::index_sequence_for<Args...>());
            } else {
                tq->PostTask([wself = this->weak_from_this()]() {
                    if (auto self = wself.lock()) {
                        self->invoke(std::index_sequence_for<Args...>());
                        self->sema_.signal();
                    }
                });
                // Keep waiting, never timeout
                sema_.wait();
            }
            return r_.get();
        }

    private:
        template <size_t... Is>
        void invoke(std::index_sequence<Is...>) {
            r_.invoke(c_, m_, std::move(std::get<Is>(args_))...);
        }

        const C* c_;
        Method m_;
        ReturnType<R> r_;
        std::tuple<Args&&...> args_;
        base::LightweightSemaphore sema_;
    };
}

#define PROXY_STRINGIZE_IMPL(x) #x
#define PROXY_STRINGIZE(x) PROXY_STRINGIZE_IMPL(x)

// Helper macros to reduce code duplication.
#define PROXY_MAP_BOILERPLATE(class_name, interface)                                    \
    template <class INTERNAL_CLASS>                                                     \
    class class_name##ProxyWithInternal;                                                \
    typedef class_name##ProxyWithInternal<interface> class_name##Proxy;                 \
    template <class INTERNAL_CLASS>                                                     \
    class class_name##ProxyWithInternal : public interface {                            \
    protected:                                                                          \
        static constexpr char class_name_[] = #class_name;                              \
        static constexpr char proxy_name_[] = #class_name "Proxy";                      \
        typedef interface C;                                                            \
    public:                                                                             \
        const INTERNAL_CLASS* internal() const { return c(); }                          \
        INTERNAL_CLASS* internal() { return c(); }

// clang-format off
// clang-format would put the semicolon alone,
// leading to a presubmit error (cpplint.py)
#define END_PROXY_MAP(class_name)                                                       \
    };                                                                                  \
    template <class INTERNAL_CLASS>                                                     \
    constexpr char class_name##ProxyWithInternal<INTERNAL_CLASS>::proxy_name_[];
// clang-format on

#define TASK_QUEUE_PROXY_MAP_BOILERPLATE(class_name)                                    \
    public:                                                                             \
        class_name##ProxyWithInternal(std::shared_ptr<INTERNAL_CLASS> c,                \
                                      webrtc::TaskQueueBase* task_queue)                \
        : c_(std::move(c))                                                              \
        , task_queue_(task_queue)                                                       \
        {}                                                                              \
    private:                                                                            \
        mutable webrtc::TaskQueueBase* task_queue_;

// Note that the destructor is protected so that the proxy can only be
// destroyed via RefCountInterface.
#define SHARED_PROXY_MAP_BOILERPLATE(class_name)                                        \
    public:                                                                             \
        ~class_name##ProxyWithInternal() {                                              \
            auto call =                                                                 \
            std::make_shared<details::MethodCall<class_name##ProxyWithInternal, void>>  \
            (this, &class_name##ProxyWithInternal::destroyInternal);                    \
            call->marshal(destructor_queue());                                          \
        }                                                                               \
    private:                                                                            \
        const INTERNAL_CLASS* c() const { return c_.get(); }                            \
        INTERNAL_CLASS* c() { return c_.get(); }                                        \
        void destroyInternal() { c_ = nullptr; }                                        \
        std::shared_ptr<INTERNAL_CLASS> c_;


#define BEGIN_PROXY_MAP(class_name, interface)                                          \
    PROXY_MAP_BOILERPLATE(class_name, interface)                                        \
    TASK_QUEUE_PROXY_MAP_BOILERPLATE(class_name)                                        \
    SHARED_PROXY_MAP_BOILERPLATE(class_name)                                            \
    public:                                                                             \
        static std::shared_ptr<class_name##ProxyWithInternal> create(                   \
        std::shared_ptr<INTERNAL_CLASS> c,                                              \
        webrtc::TaskQueueBase* task_queue) {                                            \
            return std::make_shared<class_name##ProxyWithInternal>(                     \
            std::move(c),                                                               \
            task_queue);                                                                \
        }

#define PROXY_TASK_QUEUE_DESTRUCTOR()                                                   \
private:                                                                                \
    webrtc::TaskQueueBase* destructor_queue() const { return task_queue_; }             \
public:  // NOLINTNEXTLINE

#if defined(DISABLE_PROXY_TRACE_EVENTS)
    #define TRACE_BOILERPLATE(method)                                                   \
        do {} while (0)
#else  // if defined(DISABLE_PROXY_TRACE_EVENTS)
    #define TRACE_BOILERPLATE(method)                                                   \
        static constexpr auto class_and_method_name =                                   \
        MakeCompileTimeString(proxy_name_)                                              \
        .Concat(MakeCompileTimeString("::"))                                            \
        .Concat(MakeCompileTimeString(#method));                                        \
        details::ScopedTrace scoped_trace(class_and_method_name.string)

#endif  // if defined(DISABLE_PROXY_TRACE_EVENTS)

#define PROXY_CONSTMETHOD0(r, method)                                                   \
    r method() const override {                                                         \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::ConstMethodCall<C, r>>(c(), &C::method);  \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_CONSTMETHOD1(r, method, t1)                                               \
    r method(t1 a1) const override {                                                    \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::ConstMethodCall<C, r, t1>>(               \
        c(),                                                                            \
        &C::method,                                                                     \
        std::move(a1));                                                                 \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_METHOD0(r, method)                                                        \
    r method() override {                                                               \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::MethodCall<C, r>>(c(), &C::method);       \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_METHOD1(r, method, t1)                                                    \
    r method(t1 a1) override {                                                          \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::MethodCall<C, r, t1>>(                    \
        c(),                                                                            \
        &C::method,                                                                     \
        std::move(a1));                                                                 \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_METHOD2(r, method, t1, t2)                                                \
    r method(t1 a1, t2 a2) override {                                                   \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::MethodCall<C, r, t1, t2>>(                \
        c(),                                                                            \
        &C::method,                                                                     \
        std::move(a1),                                                                  \
        std::move(a2));                                                                 \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_METHOD3(r, method, t1, t2, t3)                                            \
    r method(t1 a1, t2 a2, t3 a3) override {                                            \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::MethodCall<C, r, t1, t2, t3>>(            \
        c(),                                                                            \
        &C::method,                                                                     \
        std::move(a1),                                                                  \
        std::move(a2),                                                                  \
        std::move(a3));                                                                 \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_METHOD4(r, method, t1, t2, t3, t4)                                        \
    r method(t1 a1, t2 a2, t3 a3, t4 a4) override {                                     \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::MethodCall<C, r, t1, t2, t3, t4>>(        \
        c(),                                                                            \
        &C::method,                                                                     \
        std::move(a1),                                                                  \
        std::move(a2),                                                                  \
        std::move(a3),                                                                  \
        std::move(a4));                                                                 \
        return call->marshal(task_queue_);                                              \
    }

#define PROXY_METHOD5(r, method, t1, t2, t3, t4, t5)                                    \
    r method(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) override {                              \
        TRACE_BOILERPLATE(method);                                                      \
        auto call = std::make_shared<details::MethodCall<C, r, t1, t2, t3, t4, t5>>(    \
        c(),                                                                            \
        &C::method,                                                                     \
        std::move(a1),                                                                  \
        std::move(a2),                                                                  \
        std::move(a3),                                                                  \
        std::move(a4),                                                                  \
        std::move(a5));                                                                 \
        return call->marshal(task_queue_);                                              \
    }

#define BYPASS_PROXY_METHOD0(r, method)                                                 \
    r method() override {                                                               \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method();                                                            \
    }

#define BYPASS_PROXY_METHOD1(r, method, t1)                                             \
    r method(t1 a1) override {                                                          \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method(                                                              \
        std::move(a1));                                                                 \
    }

#define BYPASS_PROXY_METHOD2(r, method, t1, t2)                                         \
    r method(t1 a1, t2 a2) override {                                                   \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method(                                                              \
        std::move(a1),                                                                  \
        std::move(a2));                                                                 \
    }

#define BYPASS_PROXY_METHOD3(r, method, t1, t2, t3)                                     \
    r method(t1 a1, t2 a2, t3 a3) override {                                            \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method(                                                              \
        std::move(a1),                                                                  \
        std::move(a2),                                                                  \
        std::move(a3));                                                                 \
    }

#define BYPASS_PROXY_METHOD4(r, method, t1, t2, t3, t4)                                 \
    r method(t1 a1, t2 a2, t3 a3, t4 a4) override {                                     \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method(                                                              \
        std::move(a1),                                                                  \
        std::move(a2),                                                                  \
        std::move(a3),                                                                  \
        std::move(a4));                                                                 \
    }

#define BYPASS_PROXY_METHOD5(r, method, t1, t2, t3, t4, t5)                             \
    r method(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) override {                              \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method(                                                              \
        std::move(a1),                                                                  \
        std::move(a2),                                                                  \
        std::move(a3),                                                                  \
        std::move(a4),                                                                  \
        std::move(a5));                                                                 \
    }

// For use when returning purely const state (set during construction).
// Use with caution. This method should only be used when the return value will
// always be the same.
#define BYPASS_PROXY_CONSTMETHOD0(r, method)                                            \
    r method() const override {                                                         \
        TRACE_BOILERPLATE(method);                                                      \
        return c_->method();                                                            \
    }

}  // namespace base

#endif  //  _PROXY_H_
