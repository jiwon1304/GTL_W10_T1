#pragma once
#include "Container/Array.h"
#include "Container/Set.h"
#include "Core/Misc/Spinlock.h"

class UClass;
class UObject;


class FUObjectArray
{
public:
    void AddObject(UObject* Object);
    bool MarkRemoveObject(UObject* Object);

    void ProcessPendingDestroyObjects();

    TSet<UObject*>& GetObjectItemArrayUnsafe()
    {
        return ObjObjects;
    }

    const TSet<UObject*>& GetObjectItemArrayUnsafe() const
    {
        return ObjObjects;
    }

private:
    TSet<UObject*> ObjObjects;
    TArray<UObject*> PendingDestroyObjects;

    FSpinLock Lock;
};

extern FUObjectArray GUObjectArray;
