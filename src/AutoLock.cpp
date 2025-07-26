#include "AutoLock.h"

LockMutex::LockMutex()
{
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE); //同线程可多次进入
    pthread_mutex_init(&m_mutex, &mutexAttr);
}

LockMutex::~LockMutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void LockMutex::lock()
{
    pthread_mutex_lock(&m_mutex);
}

void LockMutex::unlock()
{
    pthread_mutex_unlock(&m_mutex);
}


AutoMutex::AutoMutex(LockMutex* pLock)
{
    m_lock = pLock;

    if(m_lock)
    {
        m_lock->lock();
    }
}

AutoMutex::~AutoMutex()
{
    if(m_lock)
    {
        m_lock->unlock();
    }
}

LockRW::LockRW()
{
    pthread_rwlock_init(&m_lock, NULL);
}

LockRW::~LockRW()
{
    pthread_rwlock_destroy(&m_lock);
}

void LockRW::rlock()
{
    pthread_rwlock_rdlock(&m_lock);
}

void LockRW::wlock()
{
    pthread_rwlock_wrlock(&m_lock);
}

void LockRW::unlock()
{
    pthread_rwlock_unlock(&m_lock);
}

AutoRLock::AutoRLock(LockRW* rlock)
{
    m_pRlock = rlock;

    if(m_pRlock != NULL)
    {
        m_pRlock->rlock();
    }
}

AutoRLock::~AutoRLock()
{
    if(m_pRlock != NULL)
    {
        m_pRlock->unlock();
    }
}

AutoWLock::AutoWLock(LockRW* wlock)
{
    m_pWlock = wlock;

    if(m_pWlock != NULL)
    {
        m_pWlock->wlock();
    }
}

AutoWLock::~AutoWLock()
{
    if(m_pWlock != NULL)
    {
        m_pWlock->unlock();
    }
}
