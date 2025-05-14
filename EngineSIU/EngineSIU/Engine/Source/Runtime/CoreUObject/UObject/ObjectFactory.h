#pragma once
#include "Core/Misc/Spinlock.h"
#include "EngineStatics.h"
#include "Object.h"
#include "Class.h"
#include "Define.h"
#include "UObjectArray.h"
#include "UserInterface/Console.h"

class FObjectFactory
{
public:
    static UObject* ConstructObject(UClass* InClass, UObject* InOuter, FName InName = NAME_None)
    {
        const uint32 Id = UEngineStatics::GenUUID();
        FString Name = InClass->GetName() + "_" + std::to_string(Id);
        if (InName != NAME_None)
        {
            Name = InName.ToString();
        }

        UObject* Obj = InClass->ClassCTOR();
        Obj->ClassPrivate = InClass;
        Obj->NamePrivate = Name;
        Obj->UUID = Id;
        Obj->OuterPrivate = InOuter;

        GUObjectArray.AddObject(Obj);

        UE_LOG(ELogLevel::Display, "Created New Object : %s", *Name);
        return Obj;
    }

    template<typename T>
        requires std::derived_from<T, UObject>
    static T* ConstructObject(UObject* InOuter)
    {
        return static_cast<T*>(ConstructObject(T::StaticClass(), InOuter));
    }

    static void DeleteObject(UObject* Object)
    {
        // Nullptr check
        if (!Object)
        {
            UE_LOG(ELogLevel::Warning, "Attempted to delete a null object.");
            return;
        }

        // Check if the object exists in GUObjectArray
        if (!GUObjectArray.MarkRemoveObject(Object))
        {
            UE_LOG(ELogLevel::Warning, "Object not found in GUObjectArray: %s", *Object->GetName());
            return;
        }

        UE_LOG(ELogLevel::Display, "Deleted Object : %s", *Object->GetName());
    }
};
