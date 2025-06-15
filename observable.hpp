#pragma once

#include <type_traits>
#include <list>
#include <algorithm>
#include <any>
#include <mutex>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <chrono>
#include <string>
#include <sstream>
#include <vector>
#include <cassert>
#include <memory>
#include <atomic>
#include "api/task_queue/task_queue_base.h"

namespace observable {
// 观察者优先级
enum class ObserverPriority {
    High,     // 高优先级
    Normal,   // 正常优先级
    Low       // 低优先级
};

// 通知选项
struct NotificationOptions {
    // 处理通知错误的回调
    std::function<void(const std::exception&, const std::string&)> errorHandler;

    // 是否跳过弱引用清理（在你确定没有过期的弱引用时，可以提高性能）
    bool skipWeakRefCleanup = false;

    // 是否保留通知顺序（默认按优先级通知）
    bool preserveOrder = false;
};

// 观察者标识符 - 用于不需要保留观察者实例的移除操作
class ObserverToken {
public:
    ObserverToken() : id(nextId.fetch_add(1, std::memory_order_relaxed)) {
        // 处理潜在的溢出（虽然极不可能发生）
        if (id == 0) id = nextId.fetch_add(1, std::memory_order_relaxed);
    }
    ObserverToken(const ObserverToken&) = delete;
    ObserverToken& operator=(const ObserverToken&) = delete;
    ObserverToken(ObserverToken&&) = default;
    ObserverToken& operator=(ObserverToken&&) = default;

    bool operator==(const ObserverToken& other) const {
        return id == other.id;
    }

    size_t getId() const { return id; }

private:
    size_t id;
    static std::atomic<size_t> nextId;
};

std::atomic<size_t> ObserverToken::nextId{1};

template<typename Observer>
class Observable {
public:
    using ObserverPtr = std::shared_ptr<Observer>;
    using ObserverWeakPtr = std::weak_ptr<Observer>;
    using CallbackType = std::function<void(const ObserverPtr&)>;
    using PredicateType = std::function<bool(const ObserverPtr&)>;

    // 构造函数
    Observable() = default;

    // 禁止复制和移动
    Observable(const Observable&) = delete;
    Observable& operator=(const Observable&) = delete;
    Observable(Observable&&) = delete;
    Observable& operator=(Observable&&) = delete;

    // 析构函数
    virtual ~Observable() {
        clearObservers();
    }

    // 创建观察者标记（便于后续移除）
    ObserverToken createToken() {
        return ObserverToken();
    }

    // 添加带队列的强引用观察者（带标记）
    void addObserver(const ObserverPtr &observer,
                     webrtc::TaskQueueBase* taskQ,
                     ObserverPriority priority = ObserverPriority::Normal,
                     const ObserverToken* token = nullptr) {
        if (!observer) return;

        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (!_hasObserver_nolock(observer)) {
            StrongRef sref(observer, taskQ, priority, token ? token->getId() : 0);
            _insertWithPriority(sref);
        }
    }

    // 添加带队列的弱引用观察者（带标记）
    void addObserverWeakRef(const ObserverPtr& observer,
                            webrtc::TaskQueueBase* taskQ,
                            ObserverPriority priority = ObserverPriority::Normal,
                            const ObserverToken* token = nullptr) {
        if (!observer) return;

        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (!_hasObserver_nolock(observer)) {
            WeakRef wref(observer, taskQ, priority, token ? token->getId() : 0);
            _insertWithPriority(wref);
        }
    }

    // 添加不带队列的观察者（便利方法）
    void addObserver(const ObserverPtr &observer,
                     ObserverPriority priority = ObserverPriority::Normal,
                     const ObserverToken* token = nullptr) {
        addObserver(observer, nullptr, priority, token);
    }

    // 添加不带队列的弱引用观察者（便利方法）
    void addObserverWeakRef(const ObserverPtr &observer,
                            ObserverPriority priority = ObserverPriority::Normal,
                            const ObserverToken* token = nullptr) {
        addObserverWeakRef(observer, nullptr, priority, token);
    }

    // 通过标记移除观察者
    bool removeObserverByToken(const ObserverToken& token) {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        size_t sizeBefore = _observers.size();

        _observers.remove_if([&token](const std::any& val) {
            if (!val.has_value()) return false;

            size_t tokenId = 0;
            if (val.type() == typeid(WeakRef)) {
                tokenId = std::any_cast<WeakRef>(val).tokenId;
            }
            else if (val.type() == typeid(StrongRef)) {
                tokenId = std::any_cast<StrongRef>(val).tokenId;
            }

            return tokenId == token.getId();
        });

        return sizeBefore > _observers.size();
    }

    // 修改观察者优先级
    bool setObserverPriority(const ObserverPtr &observer, ObserverPriority priority) {
        if (!observer) return false;

        std::unique_lock<std::shared_mutex> lock(_mutex);

        // 查找并修改观察者优先级
        auto it = _findObserver_nolock(observer);
        if (it == _observers.end()) return false;

        // 创建新引用并更新优先级
        if (it->type() == typeid(WeakRef)) {
            auto wref = std::any_cast<WeakRef>(*it);
            wref.priority = priority;
            *it = wref;
        }
        else if (it->type() == typeid(StrongRef)) {
            auto sref = std::any_cast<StrongRef>(*it);
            sref.priority = priority;
            *it = sref;
        }

        // 重新排序列表
        _sortByPriority();

        return true;
    }

    // 修改观察者队列
    bool setObserverQueue(const ObserverPtr &observer, webrtc::TaskQueueBase* taskQ) {
        if (!observer) return false;

        std::unique_lock<std::shared_mutex> lock(_mutex);

        // 查找观察者
        auto it = _findObserver_nolock(observer);
        if (it == _observers.end()) return false;

        // 更新队列
        if (it->type() == typeid(WeakRef)) {
            auto wref = std::any_cast<WeakRef>(*it);
            wref.taskQ = taskQ;
            *it = wref;
        }
        else if (it->type() == typeid(StrongRef)) {
            auto sref = std::any_cast<StrongRef>(*it);
            sref.taskQ = taskQ;
            *it = sref;
        }

        return true;
    }

    // 移除观察者
    void removeObserver(const ObserverPtr &observer) {
        if (!observer) return;

        std::unique_lock<std::shared_mutex> lock(_mutex);
        _observers.remove_if([&observer](const std::any& val) {
            if (!val.has_value()) return false;

            if (val.type() == typeid(WeakRef)) {
                auto wref = std::any_cast<WeakRef>(val);
                auto locked = wref.observer.lock();
                return locked && locked == observer;
            }
            else if (val.type() == typeid(StrongRef)) {
                auto sref = std::any_cast<StrongRef>(val);
                return sref.observer == observer;
            }

            return false;
        });
    }

    // 批量添加观察者
    void addObservers(const std::vector<ObserverPtr>& observers,
                      webrtc::TaskQueueBase* taskQ = nullptr,
                      ObserverPriority priority = ObserverPriority::Normal) {
        if (observers.empty()) return;

        std::unique_lock<std::shared_mutex> lock(_mutex);

        for (const auto& observer : observers) {
            if (!observer || _hasObserver_nolock(observer)) continue;

            StrongRef sref(observer, taskQ, priority);
            _insertWithPriority(sref);
        }
    }

    // 批量添加弱引用观察者
    void addObserversWeakRef(const std::vector<ObserverPtr>& observers,
                             webrtc::TaskQueueBase* taskQ = nullptr,
                             ObserverPriority priority = ObserverPriority::Normal) {
        if (observers.empty()) return;

        std::unique_lock<std::shared_mutex> lock(_mutex);

        for (const auto& observer : observers) {
            if (!observer || _hasObserver_nolock(observer)) continue;

            WeakRef wref(observer, taskQ, priority);
            _insertWithPriority(wref);
        }
    }

    // 清除所有观察者
    void clearObservers() {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        _observers.clear();
    }

    // 获取观察者数量
    size_t numOfObservers() const {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        return _countValidObservers_nolock();
    }

    // 检查是否包含特定观察者
    bool hasObserver(const ObserverPtr &observer) const {
        if (!observer) return false;

        std::shared_lock<std::shared_mutex> lock(_mutex);
        return _hasObserver_nolock(observer);
    }

    // 检查是否为空
    bool isEmpty() const {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        return _countValidObservers_nolock() == 0;
    }

    // 设置多久执行一次弱引用清理（n = 每 n 次通知）
    void setWeakRefCleanupFrequency(size_t frequency) {
        _weakRefCleanupFrequency = frequency > 0 ? frequency : 1;
    }

    // 获取当前清理频率
    size_t getWeakRefCleanupFrequency() const {
        return _weakRefCleanupFrequency;
    }

protected:
    // 查找观察者（无锁版本）
    auto _findObserver_nolock(const ObserverPtr &observer) const {
        return std::find_if(_observers.begin(), _observers.end(),
                            [&observer](const std::any& val) {
            if (!val.has_value()) return false;

            if (val.type() == typeid(WeakRef)) {
                auto wref = std::any_cast<WeakRef>(val);
                auto locked = wref.observer.lock();
                return locked && locked == observer;
            }
            else if (val.type() == typeid(StrongRef)) {
                auto sref = std::any_cast<StrongRef>(val);
                return sref.observer == observer;
            }

            return false;
        });
    }

    // 计算有效观察者数量（无锁版本）
    size_t _countValidObservers_nolock() const {
        size_t count = 0;
        for (const auto& ref : _observers) {
            if (!ref.has_value()) continue;

            if (ref.type() == typeid(WeakRef)) {
                auto wref = std::any_cast<WeakRef>(ref);
                if (!wref.observer.expired()) count++;
            } else {
                count++;
            }
        }
        return count;
    }

    // 内部检查观察者是否存在（无锁版本）
    bool _hasObserver_nolock(const ObserverPtr &observer) const {
        return _findObserver_nolock(observer) != _observers.end();
    }

    // 根据优先级插入
    template <typename RefType>
    void _insertWithPriority(const RefType& ref) {
        // 如果列表为空或者是低优先级，直接添加到末尾
        if (_observers.empty() || ref.priority == ObserverPriority::Low) {
            _observers.emplace_back(ref);
            return;
        }

        // 否则根据优先级插入
        auto it = _observers.begin();
        while (it != _observers.end()) {
            std::any& val = *it;

            ObserverPriority currentPriority = ObserverPriority::Normal;

            if (val.type() == typeid(WeakRef)) {
                currentPriority = std::any_cast<WeakRef>(val).priority;
            }
            else if (val.type() == typeid(StrongRef)) {
                currentPriority = std::any_cast<StrongRef>(val).priority;
            }

            // 高优先级添加到前面，同等优先级添加到后面
            if (ref.priority > currentPriority) {
                _observers.insert(it, ref);
                return;
            }

            ++it;
        }

        // 如果到这里还没有插入，添加到末尾
        _observers.emplace_back(ref);
    }

    // 根据优先级排序
    void _sortByPriority() {
        _observers.sort([](const std::any& a, const std::any& b) {
            ObserverPriority priorityA = ObserverPriority::Normal;
            ObserverPriority priorityB = ObserverPriority::Normal;

            if (a.type() == typeid(WeakRef)) {
                priorityA = std::any_cast<WeakRef>(a).priority;
            }
            else if (a.type() == typeid(StrongRef)) {
                priorityA = std::any_cast<StrongRef>(a).priority;
            }

            if (b.type() == typeid(WeakRef)) {
                priorityB = std::any_cast<WeakRef>(b).priority;
            }
            else if (b.type() == typeid(StrongRef)) {
                priorityB = std::any_cast<StrongRef>(b).priority;
            }

            // 高优先级在前
            return priorityA > priorityB;
        });
    }

    // 清理失效的弱引用
    size_t _cleanupWeakReferences() {
        size_t beforeSize = _observers.size();
        _observers.remove_if([](const std::any& val) {
            if (!val.has_value()) return true;

            if (val.type() == typeid(WeakRef)) {
                auto wref = std::any_cast<WeakRef>(val);
                return wref.observer.expired();
            }

            return false;
        });

        return beforeSize - _observers.size();
    }

public:
    // 通知所有观察者（带选项）
    virtual void notifyObservers(CallbackType callback,
                                 const NotificationOptions& options = {}) {
        if (!callback) {
            assert(false && "Observer callback cannot be null");
            return;
        }

        // 获取观察者的快照，避免在通知期间修改列表
        std::vector<std::any> observersCopy;
        {
            // 首先检查是否需要清理弱引用
            if (!options.skipWeakRefCleanup &&
                    (++_notificationCounter % _weakRefCleanupFrequency == 0)) {
                std::unique_lock<std::shared_mutex> wlock(_mutex);
                size_t cleanedCount = _cleanupWeakReferences();

                if (_debug && cleanedCount > 0) {
                    _logDebug("Cleaned up " + std::to_string(cleanedCount) + " expired weak references");
                }

                // 持有写锁的同时复制观察者列表
                observersCopy.reserve(_observers.size());
                observersCopy.assign(_observers.begin(), _observers.end());
            } else {
                // 如果不需要清理，只需使用读锁
                std::shared_lock<std::shared_mutex> lock(_mutex);
                observersCopy.reserve(_observers.size());
                observersCopy.assign(_observers.begin(), _observers.end());
            }
        }

        _notifyObserversImpl(observersCopy, callback, nullptr, options);
    }

    // 通知特定条件的观察者（带选项）
    virtual void notifyObserversIf(PredicateType predicate,
                                   CallbackType callback,
                                   const NotificationOptions& options = {}) {
        if (!callback || !predicate) {
            assert(false && "Observer callback and predicate cannot be null");
            return;
        }

        // 获取观察者的快照，避免在通知期间修改列表
        std::vector<std::any> observersCopy;
        {
            // 首先检查是否需要清理弱引用
            if (!options.skipWeakRefCleanup &&
                    (++_notificationCounter % _weakRefCleanupFrequency == 0)) {
                std::unique_lock<std::shared_mutex> wlock(_mutex);
                _cleanupWeakReferences();

                // 持有写锁的同时复制观察者列表
                observersCopy.reserve(_observers.size());
                observersCopy.assign(_observers.begin(), _observers.end());
            } else {
                // 如果不需要清理，只需使用读锁
                std::shared_lock<std::shared_mutex> lock(_mutex);
                observersCopy.reserve(_observers.size());
                observersCopy.assign(_observers.begin(), _observers.end());
            }
        }

        _notifyObserversImpl(observersCopy, callback, predicate, options);
    }

    // 批量通知
    virtual void notifyObserversBatch(const std::vector<std::pair<PredicateType, CallbackType>>& notifications,
                                      const NotificationOptions& options = {}) {
        if (notifications.empty()) return;

        // 校验回调和谓词
        for (const auto& [predicate, callback] : notifications) {
            if (!callback) {
                assert(false && "Observer callback cannot be null");
                return;
            }
        }

        // 获取观察者的快照
        std::vector<std::any> observersCopy;
        {
            // 首先检查是否需要清理弱引用
            if (!options.skipWeakRefCleanup &&
                    (++_notificationCounter % _weakRefCleanupFrequency == 0)) {
                std::unique_lock<std::shared_mutex> wlock(_mutex);
                _cleanupWeakReferences();

                // 持有写锁的同时复制观察者列表
                observersCopy.reserve(_observers.size());
                observersCopy.assign(_observers.begin(), _observers.end());
            } else {
                // 如果不需要清理，只需使用读锁
                std::shared_lock<std::shared_mutex> lock(_mutex);
                observersCopy.reserve(_observers.size());
                observersCopy.assign(_observers.begin(), _observers.end());
            }
        }

        for (const auto& [predicate, callback] : notifications) {
            _notifyObserversImpl(observersCopy, callback, predicate, options);
        }
    }

private:
    // 通知实现（带有可选的过滤谓词和选项）
    void _notifyObserversImpl(const std::vector<std::any>& observers,
                              CallbackType callback,
                              PredicateType predicate,
                              const NotificationOptions& options) {
        // 统计已通知的观察者数量（用于调试）
        size_t notifiedCount = 0;

        // 准备错误处理器，如果没有提供则使用默认值
        auto errorHandler = options.errorHandler ? options.errorHandler :
                                                   [this](const std::exception& e, const std::string& context) {
            if (_debug) {
                std::cerr << "观察者通知错误: " << e.what()
                          << " (上下文: " << context << ")" << std::endl;
            }
        };

        // 应用 preserveOrder 选项
        std::vector<std::any> sortedObservers;
        if (options.preserveOrder) {
            // 使用原始顺序
            sortedObservers = observers;
        } else {
            // 按优先级排序（高优先级优先）
            sortedObservers = observers;
            std::sort(sortedObservers.begin(), sortedObservers.end(),
                      [](const std::any& a, const std::any& b) {
                ObserverPriority priorityA = ObserverPriority::Normal;
                ObserverPriority priorityB = ObserverPriority::Normal;

                if (a.has_value()) {
                    if (a.type() == typeid(WeakRef)) {
                        priorityA = std::any_cast<WeakRef>(a).priority;
                    } else if (a.type() == typeid(StrongRef)) {
                        priorityA = std::any_cast<StrongRef>(a).priority;
                    }
                }

                if (b.has_value()) {
                    if (b.type() == typeid(WeakRef)) {
                        priorityB = std::any_cast<WeakRef>(b).priority;
                    } else if (b.type() == typeid(StrongRef)) {
                        priorityB = std::any_cast<StrongRef>(b).priority;
                    }
                }

                // 高优先级优先
                return priorityA > priorityB;
            });
        }

        // 使用可能已排序的观察者列表
        for (const auto &ref: sortedObservers) {
            if (!ref.has_value()) continue;

            std::shared_ptr<Observer> observer = nullptr;
            webrtc::TaskQueueBase* taskQ = nullptr;

            try {
                if (ref.type() == typeid(WeakRef)) {
                    auto wref = std::any_cast<WeakRef>(ref);
                    observer = wref.observer.lock();
                    taskQ = wref.taskQ;
                } else if (ref.type() == typeid(StrongRef)) {
                    auto sref = std::any_cast<StrongRef>(ref);
                    observer = sref.observer;
                    taskQ = sref.taskQ;
                }
            } catch (const std::exception& e) {
                errorHandler(e, "Type casting error");
                continue;
            }

            if (!observer) continue;

            // 如果有谓词，使用它过滤观察者
            try {
                if (predicate && !predicate(observer)) {
                    continue;
                }
            } catch (const std::exception& e) {
                std::stringstream ss;
                ss << "Predicate error for observer " << observer.get();
                errorHandler(e, ss.str());
                continue;
            }

            notifiedCount++;

            if (!taskQ || taskQ->IsCurrent()) {
                // 同步调用
                try {
                    callback(observer);
                } catch (const std::exception& e) {
                    std::stringstream ss;
                    ss << "Callback error for observer " << observer.get();
                    errorHandler(e, ss.str());
                } catch (...) {
                    try {
                        std::throw_with_nested(std::runtime_error("Unknown callback error"));
                    } catch (const std::exception& e) {
                        std::stringstream ss;
                        ss << "Unknown error for observer " << observer.get();
                        errorHandler(e, ss.str());
                    }
                }
            } else {
                // 异步调用
                taskQ->PostTask([observer, callback, errorHandler]() {
                    try {
                        callback(observer);
                    } catch (const std::exception& e) {
                        std::stringstream ss;
                        ss << "Async callback error for observer " << observer.get();
                        errorHandler(e, ss.str());
                    } catch (...) {
                        try {
                            std::throw_with_nested(std::runtime_error("Unknown async callback error"));
                        } catch (const std::exception& e) {
                            std::stringstream ss;
                            ss << "Unknown async error for observer " << observer.get();
                            errorHandler(e, ss.str());
                        }
                    }
                });
            }
        }

        // 可选：添加调试输出
        if (notifiedCount > 0) {
            _logDebug("Notified " + std::to_string(notifiedCount) + " observers");
        }
    }

public:
    // 启用或禁用调试模式
    void setDebugMode(bool enable) {
        _debug = enable;
    }

    // 检查调试模式状态
    bool isDebugModeEnabled() const {
        return _debug;
    }

    // 获取所有观察者的信息（用于调试）
    std::string getObserversInfo() const {
        std::stringstream ss;
        size_t validCount = 0;
        size_t expiredCount = 0;

        std::shared_lock<std::shared_mutex> lock(_mutex);

        ss << "Observer information:" << std::endl;
        ss << "-------------------" << std::endl;

        for (const auto& ref : _observers) {
            if (!ref.has_value()) continue;

            if (ref.type() == typeid(WeakRef)) {
                auto wref = std::any_cast<WeakRef>(ref);
                auto observer = wref.observer.lock();

                if (observer) {
                    ss << "WeakRef (active) - " << observer.get() << ", Queue: "
                       << (wref.taskQ ? (wref.taskQ->Name().empty() ? "unnamed" : wref.taskQ->Name()) : "none")
                       << ", Priority: ";

                    switch (wref.priority) {
                    case ObserverPriority::High: ss << "High"; break;
                    case ObserverPriority::Normal: ss << "Normal"; break;
                    case ObserverPriority::Low: ss << "Low"; break;
                    }

                    if (wref.tokenId != 0) {
                        ss << ", Token: " << wref.tokenId;
                    }
                    ss << "\n";

                    validCount++;
                } else {
                    ss << "WeakRef (expired)";
                    if (wref.tokenId != 0) {
                        ss << ", Token: " << wref.tokenId;
                    }
                    ss << "\n";
                    expiredCount++;
                }
            } else if (ref.type() == typeid(StrongRef)) {
                auto sref = std::any_cast<StrongRef>(ref);

                ss << "StrongRef - " << sref.observer.get() << ", Queue: "
                   << (sref.taskQ ? (sref.taskQ->Name().empty() ? "unnamed" : sref.taskQ->Name()) : "none")
                   << ", Priority: ";

                switch (sref.priority) {
                case ObserverPriority::High: ss << "High"; break;
                case ObserverPriority::Normal: ss << "Normal"; break;
                case ObserverPriority::Low: ss << "Low"; break;
                }

                if (sref.tokenId != 0) {
                    ss << ", Token: " << sref.tokenId;
                }
                ss << "\n";

                validCount++;
            }
        }

        ss << "-------------------" << std::endl;
        ss << "Total: " << validCount << " active observers, " << expiredCount << " expired" << std::endl;

        return ss.str();
    }

    // 执行弱引用清理并返回清理的引用数量
    size_t performWeakRefCleanup() {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        return _cleanupWeakReferences();
    }

    // 添加这个新方法来获取统计信息
    std::string getStatistics() const {
        // 先获取观察者数量，避免嵌套锁
        size_t observerCount = numOfObservers();

        std::stringstream ss;
        {
            std::lock_guard<std::mutex> lock(_debugLogMutex);
            ss << "Observable Statistics:" << std::endl;
            ss << "  Total observers: " << observerCount << std::endl;
            ss << "  Cleanup frequency: Every " << _weakRefCleanupFrequency << " notifications" << std::endl;
            ss << "  Current notification counter: " << _notificationCounter << std::endl;
        }
        return ss.str();
    }

    // 安全清理所有资源（用于应用程序退出前）
    void safeShutdown() {
        {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            _observers.clear();
        }
        _logDebug("Observable safely shut down, all observers cleared");
    }

    // 兼容旧代码的辅助方法
    void forceCleanup() {
        performWeakRefCleanup();
    }

    // 检查是否有任何弱引用过期
    bool hasExpiredWeakRefs() const {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        for (const auto& ref : _observers) {
            if (ref.has_value() && ref.type() == typeid(WeakRef)) {
                auto wref = std::any_cast<WeakRef>(ref);
                if (wref.observer.expired()) return true;
            }
        }
        return false;
    }

    // 预先分配观察者列表容量（优化性能）
    void reserveCapacity(size_t capacity) {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        // 虽然 std::list 没有 reserve 方法，但我们可以提前创建一个自定义容器
        if (capacity > 0 && _observers.empty()) {
            _logDebug("Optimizing for expected capacity: " + std::to_string(capacity));
            // 可选：使用自定义的内存分配策略
        }
    }

private:
    // Observer wrapper
    template<typename T = std::shared_ptr<Observer>>
    class Wrapper {
    public:
        Wrapper(std::shared_ptr<Observer> o,
                webrtc::TaskQueueBase* q,
                ObserverPriority p = ObserverPriority::Normal,
                size_t id = 0)
            : observer(o), taskQ(q), priority(p), tokenId(id) {}

        T observer;
        webrtc::TaskQueueBase* taskQ = nullptr;
        ObserverPriority priority = ObserverPriority::Normal;
        size_t tokenId = 0; // 用于通过标记查找和移除观察者
    };

    using WeakRef = Wrapper<std::weak_ptr<Observer>>;
    using StrongRef = Wrapper<std::shared_ptr<Observer>>;

    mutable std::shared_mutex _mutex;
    std::list<std::any> _observers;
    bool _debug = false;
    size_t _weakRefCleanupFrequency = 1; // 默认：每次通知都清理
    std::atomic<size_t> _notificationCounter{0};
    mutable std::mutex _debugLogMutex;

    // 改进调试日志记录
    void _logDebug(const std::string& message) const {
        if (_debug) {
            std::lock_guard<std::mutex> lock(_debugLogMutex);
            std::cout << "[Observable] " << message << std::endl;
        }
    }
};
}


