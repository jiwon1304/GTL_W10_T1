#pragma once
#include <atomic>

class FSpinLock
{
public:
    FSpinLock() = default;

    void Lock()
    {
        while (true)
        {
            if (!bFlag.exchange(true, std::memory_order_acquire))
            {
                break;
            }
        }
    }

    bool TryLock()
    {
        return !bFlag.exchange(true, std::memory_order_acquire);
    }

    void Unlock()
    {
        bFlag.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> bFlag{ false };
};

class FSpinLockGuard
{
public:
    explicit FSpinLockGuard(FSpinLock& InLock)
        : LockRef(InLock)
    {
        LockRef.Lock();
    }

    ~FSpinLockGuard()
    {
        LockRef.Unlock();
    }

    // 복사 금지
    FSpinLockGuard(const FSpinLockGuard&) = delete;
    FSpinLockGuard& operator=(const FSpinLockGuard&) = delete;

private:
    FSpinLock& LockRef;
};
