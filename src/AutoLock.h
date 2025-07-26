#ifndef SRC_SDK_AUTOLOCK_H_
#define SRC_SDK_AUTOLOCK_H_

#include <pthread.h>
//  互斥锁实现
class LockMutex
{
public:
    LockMutex();
    virtual ~LockMutex();

public:
    void lock();
    void unlock();

private:
    pthread_mutex_t m_mutex;
};
//  RAII 互斥锁管理
class AutoMutex
{
public:
    AutoMutex(LockMutex* pLock);
    virtual ~AutoMutex();

private:
    LockMutex*  m_lock;
};
//  读写锁实现
class LockRW
{
public:
    LockRW();
    virtual ~LockRW();

public:
    void rlock();
    void wlock();
    void unlock();

private:
    pthread_rwlock_t m_lock;
};
//  RAII 读写锁的管理
class AutoRLock
{
public:
    AutoRLock(LockRW* rlock);
    virtual ~AutoRLock();

private:
    LockRW* m_pRlock;
};

class AutoWLock
{
public:
    AutoWLock(LockRW* wlock);
    virtual ~AutoWLock();

private:
    LockRW* m_pWlock;
};

#endif /* SRC_SDK_AUTOLOCK_H_ */
