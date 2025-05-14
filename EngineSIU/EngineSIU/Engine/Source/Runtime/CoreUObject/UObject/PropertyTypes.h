#pragma once
#include <concepts>
#include "HAL/PlatformType.h"
#include "Core/Container/Array.h"
#include "Core/Container/Map.h"
#include "Core/Container/Set.h"
#include "Template/IsArray.h"

enum EPropertyFlags : uint32
{
    None = 0,
    EditAnywhere = 1 << 0,
    BlueprintReadOnly = 1 << 1,
    BlueprintReadWrite = 1 << 2,
    VisibleAnywhere = 1 << 3,
};
using enum EPropertyFlags;

enum class EPropertyType : uint8
{
    Unknown,                       // 알 수 없는 타입
    UnresolvedPointer,             // 컴파일 타임에 알 수 없는 포인터 타입
    Int8, Int16, Int32, Int64,     // 부호 있는 정수타입
    UInt8, UInt16, UInt32, UInt64, // 부호 없는 정수타입
    Float, Double,                 // 실수 타입
    Bool,                          // Boolean 타입
    String,                        // 문자열 타입 (FString)
    Name,                          // 이름 타입 (FName)
    Array,                         // TArray<T>
    Map,                           // TMap<T>
    Set,                           // TSet<T>
    Enum,                          // 커스텀 Enum 타입
    Object,                        // UObject* 타입
    Struct,                        // 사용자 정의 구조체 타입
    Rotator,
    Vector,
    Color,                         // 색상 타입 (FColor)
};

template <typename T>
consteval EPropertyType GetPropertyType()
{
    // 기본 내장 타입들
    if constexpr (std::same_as<T, int8>) { return EPropertyType::Int8; }
    else if constexpr (std::same_as<T, int16>) { return EPropertyType::Int16; }
    else if constexpr (std::same_as<T, int32>) { return EPropertyType::Int32; }
    else if constexpr (std::same_as<T, int64>) { return EPropertyType::Int64; }
    else if constexpr (std::same_as<T, uint8>) { return EPropertyType::UInt8; }
    else if constexpr (std::same_as<T, uint16>) { return EPropertyType::UInt16; }
    else if constexpr (std::same_as<T, uint32>) { return EPropertyType::UInt32; }
    else if constexpr (std::same_as<T, uint64>) { return EPropertyType::UInt64; }
    else if constexpr (std::same_as<T, float>) { return EPropertyType::Float; }
    else if constexpr (std::same_as<T, double>) { return EPropertyType::Double; }
    else if constexpr (std::same_as<T, bool>) { return EPropertyType::Bool; }

    // 엔진 기본 타입들
    else if constexpr (std::same_as<T, FString>) { return EPropertyType::String; }
    else if constexpr (std::same_as<T, FName>) { return EPropertyType::Name; }

    // 포인터 타입
    else if constexpr (std::is_pointer_v<T>)
    {
        // using PointedToType = std::remove_cv_t<std::remove_pointer_t<T>>;
        //
        // // PointedToType이 완전한 타입일 때만 true를 반환.
        // // 전방 선언 시 false가 될 수 있음.
        // if constexpr (std::is_base_of_v<UObject, PointedToType>)
        // {
        //     return EPropertyType::Object;
        // }

        // T가 완전한 타입일 때만 true를 반환.
        // 전방 선언 시 false가 될 수 있음.
        if constexpr (std::derived_from<T, UObject>)
        {
            return EPropertyType::Object;
        }

        // 전방 선언된 타입이 들어올 경우, 상속관계를 확인할 수 없음
        // 이때는 UObject일 확률도 있기 때문에 런타임에 검사
        return EPropertyType::UnresolvedPointer;
    }

    // 엔진 기본 컨테이너 타입들
    else if constexpr (TIsTArray<T> || TIsArray<T>) { return EPropertyType::Array; }
    else if constexpr (TIsTMap<T>) { return EPropertyType::Map; }
    else if constexpr (TIsTSet<T>) { return EPropertyType::Set; }

    // enum만 지원
    else if constexpr (std::is_enum_v<T>) { return EPropertyType::Enum; }

    // 리플렉션 시스템에 등록된 커스텀 구조체 (StructTraits 또는 유사한 메커니즘 사용)
    // 이때 T가 UObject 값 타입이 아니어야 함 (UObject 값 타입은 보통 프로퍼티로 사용 안 함)
    // else if constexpr (TStructTraits<T>::bIsReflectedStruct && !std::is_base_of_v<UObject, T>)
    // {
    //     if (OutUnresolvedTypeName) { *OutUnresolvedTypeName = GetTypeNameString<T>(); } // 경우에 따라 이름 필요
    //     return EPropertyType::Struct; // 또는 StructTraits에 UnresolvedStruct 플래그가 있다면 그것 사용
    // }

    // 커스텀 구조체
    else if constexpr (std::is_class_v<T> && !std::derived_from<T, UObject>)
    {
        // TODO: 임시용
        // 나중에 커스텀 구조체 타입에 대해서 만들어야함
        return EPropertyType::Struct;
    }

    else
    {
        static_assert(!std::same_as<T, T>, "GetPropertyType: Type T is not supported.");
        return EPropertyType::Unknown;
    }
}

template<>
consteval EPropertyType GetPropertyType<FRotator>()
{
    return EPropertyType::Rotator;
}

template<>
consteval EPropertyType GetPropertyType<FVector>()
{
    return EPropertyType::Vector;
}

template<>
consteval EPropertyType GetPropertyType<FLinearColor>()
{
    return EPropertyType::Color;
}
