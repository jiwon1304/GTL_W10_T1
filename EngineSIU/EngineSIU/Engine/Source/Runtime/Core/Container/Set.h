#pragma once
#include <unordered_set>

#include "Array.h"
#include "ContainerAllocator.h"


template <typename T, typename Hasher = std::hash<T>, typename Allocator = FDefaultAllocator<T>>
class TSet
{
private:
    using SetType = std::unordered_set<T, Hasher, std::equal_to<>, Allocator>;
    using ElementType = T;

    SetType ContainerPrivate;

public:
    using SizeType = typename Allocator::SizeType;
    using Iterator = typename SetType::iterator;
    using ConstIterator = typename SetType::const_iterator;

    SetType& GetContainerPrivate() { return ContainerPrivate; }
    const SetType& GetContainerPrivate() const { return ContainerPrivate; }

    // 기본 생성자
    TSet() = default;

    // Iterator 관련 메서드
    Iterator begin() noexcept { return ContainerPrivate.begin(); }
    Iterator end() noexcept { return ContainerPrivate.end(); }
    ConstIterator begin() const noexcept { return ContainerPrivate.begin(); }
    ConstIterator end() const noexcept { return ContainerPrivate.end(); }

    // Add
    int32 Add(const T& Item) { return Emplace(Item); }
    int32 Add(T&& Item) { return Emplace(std::move(Item)); }

    /**
     * r-value를 받아 값을 새로 만들어 Set에 추가합니다.
     * @tparam ArgsType TSet<T>의 T부분
     * @param Args Set에 추가될 인자 (r-value)
     * @return 새로 추가된 Element의 Index, 이미 존재하는 경우 기존 Element의 Index를 반환
     */
    template<typename ArgsType = T>
    int32 Emplace(ArgsType&& Args) 
    { 
        auto iter = ContainerPrivate.emplace(std::forward<ArgsType>(Args));
        return std::distance(ContainerPrivate.begin(), iter.first);
    }

    // Num (개수)
    SizeType Num() const { return static_cast<SizeType>(ContainerPrivate.size()); }

    // Find
    Iterator Find(const T& Item) { return ContainerPrivate.find(Item); }
    ConstIterator Find(const T& Item) const { return ContainerPrivate.find(Item); }

    // Contains
    bool Contains(const T& Item) const { return ContainerPrivate.contains(Item); }

    // Array (TArray로 반환)
    TArray<T, Allocator> Array() const
    {
        TArray<T, Allocator> Result;
        Result.Reserve(Num());
        for (const auto& Item : ContainerPrivate)
        {
            Result.Add(Item);
        }
        return Result;
    }

    // Remove
    SizeType Remove(const T& Item) { return static_cast<SizeType>(ContainerPrivate.erase(Item)); }

    // Empty
    void Empty() { ContainerPrivate.clear(); }
    void Empty(SizeType Number)
    {
        ContainerPrivate.clear();
        ContainerPrivate.reserve(Number);
    }

    // IsEmpty
    bool IsEmpty() const { return ContainerPrivate.empty(); }
};

template <typename ElementType, typename Hasher, class Allocator>
FArchive& operator<<(FArchive& Ar, TSet<ElementType, Hasher, Allocator>& Set)
{
    using SizeType = typename TSet<ElementType, Hasher, Allocator>::SizeType;

    // 집합 크기 직렬화
    SizeType SetSize = Set.Num();
    Ar << SetSize;

    // 집합 요소 직렬화
    if (Ar.IsLoading())
    {
        // 로드 시 집합 초기화
        Set.Empty(SetSize);

        for (SizeType i = 0; i < SetSize; ++i)
        {
            ElementType Temp;
            Ar << Temp;
            Set.Emplace(std::move(Temp));
        }
    }
    else
    {
        for (const ElementType& Element : Set)
        {
            Ar << const_cast<ElementType&>(Element);
        }
    }

    return Ar;
}


template <typename T> constexpr bool TIsTSet_V = false;

template <typename T, typename Hasher, typename Allocator> constexpr bool TIsTSet_V<               TSet<T, Hasher, Allocator>> = true;
template <typename T, typename Hasher, typename Allocator> constexpr bool TIsTSet_V<const          TSet<T, Hasher, Allocator>> = true;
template <typename T, typename Hasher, typename Allocator> constexpr bool TIsTSet_V<      volatile TSet<T, Hasher, Allocator>> = true;
template <typename T, typename Hasher, typename Allocator> constexpr bool TIsTSet_V<const volatile TSet<T, Hasher, Allocator>> = true;

template <typename T>
concept TIsTSet = TIsTSet_V<T>;
