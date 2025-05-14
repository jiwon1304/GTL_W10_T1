#include "Class.h"
#include <cassert>

#include "EngineStatics.h"
#include "UObjectArray.h"
#include "Serialization/Archive.h"


UClass::UClass(
    const char* InClassName,
    uint32 InClassSize,
    uint32 InAlignment,
    UClass* InSuperClass,
    ClassConstructorType InCTOR
)
    : ClassCTOR(InCTOR)
    , ClassSize(InClassSize)
    , ClassAlignment(InAlignment)
    , SuperClass(InSuperClass)
{
    NamePrivate = InClassName;
}

bool UClass::IsChildOf(const UClass* SomeBase) const
{
    assert(this);
    if (!SomeBase) return false;

    // Super의 Super를 반복하면서 
    for (const UClass* TempClass = this; TempClass; TempClass=TempClass->GetSuperClass())
    {
        if (TempClass == SomeBase)
        {
            return true;
        }
    }
    return false;
}

UObject* UClass::GetDefaultObject() const
{
    if (!ClassDefaultObject)
    {
        const_cast<UClass*>(this)->CreateDefaultObject();
    }
    return ClassDefaultObject;
}

void UClass::RegisterProperty(const FProperty& Prop)
{
    Properties.Add(Prop);
}

void UClass::SerializeBin(FArchive& Ar, void* Data)
{
    if (bHasCopiedParentFields)   // 이미 복사했으면 바로 리턴
        return;

    bHasCopiedParentFields = true; // 이제 복사했다고 체크

    if (SuperClass)
    {
        SuperClass->ForEachField([&](UField* Field) {
            // 자식 리스트에 똑같은 이름의 필드가 이미 있으면 건너뛸 수도 있습니다.
            // 예) if (FindFieldByName(Field->Name)) return;

            UField* Clone = Field->Clone();
            RegisterField(Clone);
            });
    }
}

void UClass::RegisterField(UField* Field)
{
    AddField(Field);
}

/** 부모 UClass에 등록된 모든 UField 를 이 클래스에도 복사해 등록합니다. */
void UClass::CopyParentFields()
{
    if (SuperClass)   // 부모 UClass 가 있으면
    {
        SuperClass->ForEachField([&](UField* Field) {
            UField* Clone = Field->Clone();  // 깊은 복사
            RegisterField(Clone);            // UStruct::AddField
            });
    }
}

UObject* UClass::CreateDefaultObject()
{
    if (!ClassDefaultObject)
    {
        ClassDefaultObject = ClassCTOR();
        if (!ClassDefaultObject)
        {
            return nullptr;
        }

        ClassDefaultObject->ClassPrivate = this;
        ClassDefaultObject->NamePrivate = GetName() + "_CDO";
        ClassDefaultObject->UUID = UEngineStatics::GenUUID();
        GUObjectArray.AddObject(ClassDefaultObject);
    }
    return ClassDefaultObject;
}
