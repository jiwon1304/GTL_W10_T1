#include "UObjectArray.h"
#include "Object.h"
#include "UObjectHash.h"


void FUObjectArray::AddObject(UObject* Object)
{
    FSpinLockGuard LockGuard = FSpinLockGuard(Lock);
    ObjObjects.Add(Object);
    AddToClassMap(Object);
}

bool FUObjectArray::MarkRemoveObject(UObject* Object)
{
    FSpinLockGuard LockGuard = FSpinLockGuard(Lock);
    if (!ObjObjects.Contains(Object))
    {
        return false;  // Object가 존재하지 않음
    }
    ObjObjects.Remove(Object);
    RemoveFromClassMap(Object);  // UObjectHashTable에서 Object를 제외
    PendingDestroyObjects.AddUnique(Object);
    return true;
}

void FUObjectArray::ProcessPendingDestroyObjects()
{
    FSpinLockGuard LockGuard = FSpinLockGuard(Lock);
    for (UObject* Object : PendingDestroyObjects)
    {
        if (Object)
        {
            delete Object;
        }
    }
    PendingDestroyObjects.Empty();
}

FUObjectArray GUObjectArray;
