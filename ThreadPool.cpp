#include "ThreadPool.h"
#include <sstream>
#include "rtc_base/thread.h"

namespace webrtc {

class PooledThread : public TaskQueueBase
{
public:
    PooledThread(const std::string& name);
    ~PooledThread();

    void start();

    // TaskQueueBase implementation.
    void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                      const PostTaskTraits& traits,
                      const Location& location) override;

    void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                             TimeDelta delay,
                             const PostDelayedTaskTraits& traits,
                             const Location& location) override;

    bool idle();

    int idleTime();

    void stop();

    // From TaskQueueBase
    void Delete() override;

private:
    volatile bool        _idle;
    volatile std::time_t _idleTime;
    std::string          _name;
    std::unique_ptr<Thread> _thread;
    Mutex                _mutex;
};

PooledThread::PooledThread(const std::string& name):
    _idle(true),
    _idleTime(0),
    _name(name),
    _thread(Thread::Create())
{
    _thread->SetName(_name, nullptr);
    _idleTime = std::time(nullptr);
}

PooledThread::~PooledThread()
{
}


void PooledThread::start()
{
    _thread->Start();
}

inline bool PooledThread::idle()
{
    MutexLock lock(&_mutex);
    return _idle;
}


int PooledThread::idleTime()
{
    MutexLock lock(&_mutex);

    return (int) (time(nullptr) - _idleTime);
}


void PooledThread::stop()
{
    _thread->Stop();
}


// void PooledThread::activate()
// {
//     MutexLock lock(&_mutex);

//     assert(_idle);
//     _idle = false;
//     _targetCompleted.Reset();
// }


// void PooledThread::release()
// {
//     const long JOIN_TIMEOUT = 10000;

//     _mutex.lock();
//     _pTarget = nullptr;
//     _mutex.unlock();
//     // In case of a statically allocated thread pool (such
//     // as the default thread pool), Windows may have already
//     // terminated the thread before we got here.
//     if (_thread.isRunning())
//         _targetReady.Set();

//     if (_thread.tryJoin(JOIN_TIMEOUT))
//     {
//         delete this;
//     }
// }

// TaskQueueBase implementation.
void PooledThread::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                                const PostTaskTraits& traits,
                                const Location& location)
{
    _thread->PostTask(std::move(task), location);
}

void PooledThread::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                       TimeDelta delay,
                                       const PostDelayedTaskTraits& traits,
                                       const Location& location)
{
    _thread->PostDelayedTask(std::move(task), delay, location);
}

// From TaskQueueBase
void PooledThread::Delete()
{
    stop();
    delete this;
}

ThreadPool::ThreadPool(int minCapacity,
                       int maxCapacity,
                       int idleTime):
    _minCapacity(minCapacity),
    _maxCapacity(maxCapacity),
    _idleTime(idleTime),
    _serial(0),
    _age(0)
{
    assert(minCapacity >= 1 && maxCapacity >= minCapacity && idleTime > 0);

    for (int i = 0; i < _minCapacity; i++)
    {
        PooledThread* pThread = createThread();
        _threads.push_back(pThread);
        pThread->start();
    }
}


ThreadPool::ThreadPool(const std::string& name,
                       int minCapacity,
                       int maxCapacity,
                       int idleTime):
    _name(name),
    _minCapacity(minCapacity),
    _maxCapacity(maxCapacity),
    _idleTime(idleTime),
    _serial(0),
    _age(0)
{
    assert(minCapacity >= 1 && maxCapacity >= minCapacity && idleTime > 0);

    for (int i = 0; i < _minCapacity; i++)
    {
        PooledThread* pThread = createThread();
        _threads.push_back(pThread);
        pThread->start();
    }
}


ThreadPool::~ThreadPool()
{
    try
    {
        stopAll();
    }
    catch (...)
    {

    }
}


void ThreadPool::addCapacity(int n)
{
    MutexLock lock(&_mutex);

    assert(_maxCapacity + n >= _minCapacity);
    _maxCapacity += n;
    housekeep();
}


int ThreadPool::capacity() const
{
    MutexLock lock(&_mutex);
    return _maxCapacity;
}


int ThreadPool::available() const
{
    MutexLock lock(&_mutex);

    int count = 0;
    for (auto pThread: _threads)
    {
        if (pThread->idle()) ++count;
    }
    return (int) (count + _maxCapacity - _threads.size());
}


int ThreadPool::used() const
{
    MutexLock lock(&_mutex);

    int count = 0;
    for (auto pThread: _threads)
    {
        if (!pThread->idle()) ++count;
    }
    return count;
}


int ThreadPool::allocated() const
{
    MutexLock lock(&_mutex);

    return int(_threads.size());
}



void ThreadPool::stopAll()
{
    MutexLock lock(&_mutex);

    for (auto pThread: _threads)
    {
        pThread->stop();
        delete pThread;
        pThread = nullptr;
    }
    _threads.clear();
}


// void ThreadPool::joinAll()
// {
//     MutexLock lock(&_mutex);

//     for (auto pThread: _threads)
//     {
//         pThread->join();
//     }
//     housekeep();
// }


void ThreadPool::collect()
{
    MutexLock lock(&_mutex);
    housekeep();
}


void ThreadPool::housekeep()
{
    _age = 0;
    if (_threads.size() <= _minCapacity)
        return;

    ThreadVec idleThreads;
    ThreadVec expiredThreads;
    ThreadVec activeThreads;
    idleThreads.reserve(_threads.size());
    activeThreads.reserve(_threads.size());

    for (auto pThread: _threads)
    {
        if (pThread->idle())
        {
            if (pThread->idleTime() < _idleTime)
                idleThreads.push_back(pThread);
            else
                expiredThreads.push_back(pThread);
        }
        else activeThreads.push_back(pThread);
    }
    int n = (int) activeThreads.size();
    int limit = (int) idleThreads.size() + n;
    if (limit < _minCapacity) limit = _minCapacity;
    idleThreads.insert(idleThreads.end(), expiredThreads.begin(), expiredThreads.end());
    _threads.clear();
    for (auto pIdle: idleThreads)
    {
        if (n < limit)
        {
            _threads.push_back(pIdle);
            ++n;
        }
        else pIdle->stop();
    }
    _threads.insert(_threads.end(), activeThreads.begin(), activeThreads.end());
}


PooledThread* ThreadPool::getThread()
{
    MutexLock lock(&_mutex);

    if (++_age == 32)
        housekeep();

    PooledThread* pThread = nullptr;
    for (auto it = _threads.begin(); !pThread && it != _threads.end(); ++it)
    {
        if ((*it)->idle())
            pThread = *it;
    }
    if (!pThread)
    {
        if (_threads.size() < _maxCapacity)
        {
            pThread = createThread();
            try
            {
                pThread->start();
                _threads.push_back(pThread);
            }
            catch (...)
            {
                delete pThread;
                throw;
            }
        }
        else {
            //throw NoThreadAvailableException();
        }
    }
    // pThread->activate();
    return pThread;
}


PooledThread* ThreadPool::createThread()
{
    std::ostringstream name;
    name << _name << "[#" << ++_serial << "]";
    return new PooledThread(name.str());
}

// TaskQueueBase implementation.
void ThreadPool::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                              const PostTaskTraits& traits,
                              const Location& location)
{
    auto thread = getThread();
    thread->PostTaskImpl(std::move(task), traits, location);
}

void ThreadPool::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                     TimeDelta delay,
                                     const PostDelayedTaskTraits& traits,
                                     const Location& location)
{
    auto thread = getThread();
    thread->PostDelayedTaskImpl(std::move(task), delay, traits, location);
}

// From TaskQueueBase
void ThreadPool::Delete()
{
    stopAll();
    delete this;
}

class ThreadPoolSingletonHolder
{
public:
    ThreadPoolSingletonHolder()
    {
        _pPool = nullptr;
    }

    ~ThreadPoolSingletonHolder()
    {
        delete _pPool;
    }

    ThreadPool* pool()
    {
        MutexLock lock(&_mutex);

        if (!_pPool)
        {
            _pPool = new ThreadPool("default");
        }

        return _pPool;
    }

private:
    ThreadPool* _pPool;
    Mutex   _mutex;
};


ThreadPool& ThreadPool::defaultPool()
{
    static ThreadPoolSingletonHolder sh;
    return *sh.pool();
}


}
