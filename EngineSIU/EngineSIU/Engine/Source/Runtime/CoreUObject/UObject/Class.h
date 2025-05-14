#pragma once
#include <concepts>
#include "Object.h"
#include "Property.h"
#include "PropertyTypes.h"

class UField
{
public:
    UField(const FString& InName, int64 InOffset, uint32 InSize,
        EPropertyType InPropType, EPropertyFlags InFlags)
        : Next(nullptr)
        , Name(InName)
        , Offset(InOffset)
        , Size(InSize)
        , PropType(InPropType)
        , Flags(InFlags)
    {
    }
    virtual UField* Clone() const
    {
        // 기본 UField 복사
        return new UField(Name, Offset, Size, PropType, Flags);
    }


    virtual ~UField() {}

    UField*             Next;     // 링크드 리스트 다음 노드
    FString             Name;     // 변수 이름
    int64               Offset;   // offsetof(ThisClass, Var)
    uint32              Size;     // sizeof(Type)
    EPropertyType       PropType;   // 값의 타입
    EPropertyFlags      Flags;      
};

template<typename T>
class TField : public UField
{
public:
    TField(const FString& InName, int64 InOffset, uint32 InSize,
        EPropertyType InPropType, EPropertyFlags InFlags)
        : UField(InName, InOffset, InSize, InPropType, InFlags)
    {
    }

    virtual UField* Clone() const override
    {
        return new TField<T>(Name, Offset, Size, PropType, Flags);
    }

    virtual ~TField() override {}

    T GetValue(UObject* Obj) const
    {
        return *(T*)((uint8*)Obj + Offset);
    }

    void SetValue(UObject* Obj, const T& NewValue)
    {
       *(T*)((uint8*)Obj + Offset) = NewValue;
    }
};


class UStruct : public UObject
{
public:
    UStruct()
        : HeadField(nullptr)
    {}

    virtual ~UStruct() override
    {
        // 할당한 UField 모두 delete
        UField* Node = HeadField;
        while (Node)
        {
            UField* Next = Node->Next;
            delete Node;
            Node = Next;
        }
    }

    void AddField(UField* InField)
    {
        if (HeadField == nullptr)
        {
            HeadField = InField;
        }
        else
        {
            UField* Temp = HeadField;
            while (Temp->Next)
            {
                Temp = Temp->Next;
            }
            Temp->Next = InField;
        }
    }

    template<typename Func>
    void ForEachField(Func&& InFunc)
    {
        for (UField* Field = HeadField; Field != nullptr; Field = Field->Next)
        {
            InFunc(Field);
        }
    }
    void CopyParentFields();
private:
    UField* HeadField;
};

class FArchive;
/**
 * UObject의 RTTI를 가지고 있는 클래스
 */
class UClass : public UStruct
{    
    using ClassConstructorType = UObject*(*)();
public:
    UClass(
        const char* InClassName,
        uint32 InClassSize,
        uint32 InAlignment,
        UClass* InSuperClass,
        ClassConstructorType InCTOR
    );

    // 복사 & 이동 생성자 제거
    UClass(const UClass&) = delete;
    UClass& operator=(const UClass&) = delete;
    UClass(UClass&&) = delete;
    UClass& operator=(UClass&&) = delete;

    static TMap<FName, UClass*>& GetClassMap()
    {
        static TMap<FName, UClass*> ClassMap;
        return ClassMap;
    }
    static UClass* FindClass(const FName& ClassName)
    {
        auto It = GetClassMap().Find(ClassName);
        if (It)
            return *It;
        return nullptr;
    }

    uint32 GetClassSize() const { return ClassSize; }
    uint32 GetClassAlignment() const { return ClassAlignment; }

    /** SomeBase의 자식 클래스인지 확인합니다. */
    bool IsChildOf(const UClass* SomeBase) const;

    template <typename T>
        requires std::derived_from<T, UObject>
    bool IsChildOf() const;

    /**
     * 부모의 UClass를 가져옵니다.
     *
     * @note AActor::StaticClass()->GetSuperClass() == UObject::StaticClass()
     */
    UClass* GetSuperClass() const { return SuperClass; }

    UObject* GetDefaultObject() const;

    template <typename T>
        requires std::derived_from<T, UObject>
    T* GetDefaultObject() const;

    const TArray<FProperty>& GetProperties() const { return Properties; }

    /**
     * UClass에 Property를 추가합니다
     * @param Prop 추가할 Property
     */
    void RegisterProperty(const FProperty& Prop);

    /** 바이너리 직렬화 함수 */
    void SerializeBin(FArchive& Ar, void* Data);

    void RegisterField(UField* Field);

    void CopyParentFields();

protected:
    virtual UObject* CreateDefaultObject();


public:
    ClassConstructorType ClassCTOR;

private:
    uint32 ClassSize;
    uint32 ClassAlignment;

    UClass* SuperClass = nullptr;
    UObject* ClassDefaultObject = nullptr;

    TArray<FProperty> Properties;

    bool bHasCopiedParentFields = false; // 1번만 복사하도록
};

template <typename T>
    requires std::derived_from<T, UObject>
bool UClass::IsChildOf() const
{
    return IsChildOf(T::StaticClass());
}

template <typename T>
    requires std::derived_from<T, UObject>
T* UClass::GetDefaultObject() const
{
    UObject* Ret = GetDefaultObject();
    assert(Ret->IsA<T>());
    return static_cast<T*>(Ret);
}
