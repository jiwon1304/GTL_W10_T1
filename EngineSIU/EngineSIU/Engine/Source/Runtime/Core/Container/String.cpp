#include "String.h"
#include <algorithm>
#include <vector>
#include <charconv>

#include "CoreMiscDefines.h"
#include "Math/MathUtility.h"


FString FString::SanitizeFloat(float InFloat)
{
#if USE_WIDECHAR
    return FString{std::to_wstring(InFloat)};
#else
    return FString{std::to_string(InFloat)};
#endif
}

float FString::ToFloat(const FString& InString)
{
    return FCString::Atof(*InString);
}

int FString::ToInt(const FString& InString)
{
    return FCString::Atoi(*InString);
}

bool FString::ToBool() const
{
    // 빈 문자열은 false로 처리
    if (IsEmpty())
    {
        return false;
    }

    // 가장 일반적인 경우: "true" 또는 "1" (대소문자 무관)
    // Equals 함수가 이미 대소문자 무시 비교를 지원하므로 활용합니다.
    if (Equals(TEXT("true"), ESearchCase::IgnoreCase))
    {
        return true;
    }
    if (Equals(TEXT("1"))) // "1"은 대소문자 구분이 의미 없음
    {
        return true;
    }

    // 그 외: "false" 또는 "0" (대소문자 무관)
    // 이 경우들도 명시적으로 false를 반환하는 것이 안전합니다.
    if (Equals(TEXT("false"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    if (Equals(TEXT("0"))) // "0"도 대소문자 구분이 의미 없음
    {
        return false;
    }

    // 위 조건에 해당하지 않는 모든 다른 문자열은 false로 처리합니다.
    // (예: "Yes", "No", "On", "Off" 등을 추가로 지원하고 싶다면 여기에 조건을 추가할 수 있습니다.)
    // UE_LOG(LogTemp, Warning, TEXT("FString::ToBool() : Unrecognized string '%s' treated as false."), **this); // 필요시 경고 로그
    return false;
}

FString FString::RightChop(int32 Count) const
{
    const int32 MyLen = Len(); // 현재 문자열 길이

    // Count가 0 이하이면 원본 문자열의 복사본을 반환
    if (Count <= 0)
    {
        return *this; // 복사본 반환
    }

    // Count가 문자열 길이 이상이면 빈 문자열 반환
    if (Count >= MyLen)
    {
        return FString{}; // 기본 생성된 빈 FString 반환
    }

    // std::basic_string::substr(pos)는 위치 pos부터 끝까지의 부분 문자열을 반환합니다.
    // Count는 제거할 문자의 개수이므로, 부분 문자열은 Count 인덱스부터 시작합니다.
    // static_cast<size_t>는 substr이 size_t를 인자로 받기 때문에 필요합니다.
    BaseStringType Substring = PrivateString.substr(Count);

    // 추출된 부분 문자열로 새로운 FString 객체를 생성하여 반환
    // std::move를 사용하면 불필요한 복사를 피할 수 있습니다 (C++11 이상).
    return FString{std::move(Substring)};
}

void FString::Empty()
{
    PrivateString.clear();
}

bool FString::Equals(const FString& Other, ESearchCase::Type SearchCase) const
{
    const int32 Num = Len();
    const int32 OtherNum = Other.Len();

    // 길이가 다르면 무조건 false
    if (Num != OtherNum)
    {
        return false;
    }

    // 길이가 0이면 무조건 true (위에서 길이가 같다고 확인됨)
    if (Num == 0)
    {
        return true;
    }

    // 길이가 1 이상이면 내용을 비교
    if (SearchCase == ESearchCase::CaseSensitive)
    {
        return FCString::Strcmp(**this, *Other) == 0;
    }
    else // ESearchCase::IgnoreCase
    {
        return FCString::Stricmp(**this, *Other) == 0;
    }
}

bool FString::Contains(const FString& SubStr, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir) const
{
    return Find(SubStr, SearchCase, SearchDir, 0) != INDEX_NONE;
}

int32 FString::Find(
    const FString& SubStr, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir, int32 StartPosition
) const
{
    if (SubStr.IsEmpty() || IsEmpty())
    {
        return INDEX_NONE; // -1
    }

    const int32 StrLen = Len();
    const int32 SubStrLen = SubStr.Len();

    // 찾으려는 부분 문자열이 원본보다 길면 절대 찾을 수 없음
    if (SubStrLen > StrLen)
    {
        return INDEX_NONE;
    }

    const ElementType* const StrPtr = **this;
    const ElementType* const SubStrPtr = *SubStr;

    // 대소문자 비교 함수 (FCString 사용 권장)
    auto CompareFunc = [SearchCase](ElementType A, ElementType B) -> bool
    {
        if (SearchCase == ESearchCase::IgnoreCase)
        {
            return FCString::ToLower(A) == FCString::ToLower(B);
        }
        else
        {
            return A == B;
        }
    };

    // 부분 문자열을 찾는 내부 람다 함수
    // [SearchStartIndex, SearchEndIndex) 범위에서 Step 방향으로 검색
    auto FindSubString = [&](int32 SearchStartIndex, int32 SearchEndIndex, int32 Step) -> int32
    {
        for (int32 i = SearchStartIndex; i != SearchEndIndex; i += Step)
        {
            // 현재 위치 'i'에서 시작하는 부분 문자열이 SubStr과 일치하는지 확인
            bool Found = true;
            for (int32 j = 0; j < SubStrLen; ++j)
            {
                // 경계 검사: i + j 가 StrLen을 넘지 않도록 확인 (특히 FromEnd 검색 시 중요)
                // 이 로직에서는 외부에서 시작/종료 인덱스를 잘 설정하여 불필요할 수 있으나, 안전하게 추가 가능
                // if (i + j >= StrLen) { Found = false; break; }

                if (!CompareFunc(StrPtr[i + j], SubStrPtr[j]))
                {
                    Found = false;
                    break;
                }
            }
            if (Found)
            {
                return i; // 일치하는 부분 문자열의 시작 인덱스 반환
            }
        }
        return INDEX_NONE; // 찾지 못함
    };

    if (SearchDir == ESearchDir::FromStart)
    {
        // 시작 위치 보정: 0 이상, (문자열 길이 - 부분 문자열 길이) 이하
        const int32 EffectiveStartPosition = FMath::Clamp(StartPosition, 0, StrLen - SubStrLen);
        // 검색 범위: [EffectiveStartPosition, StrLen - SubStrLen + 1)
        // 마지막 가능한 시작 위치는 StrLen - SubStrLen
        return FindSubString(EffectiveStartPosition, StrLen - SubStrLen + 1, 1);
    }
    else // ESearchDir::FromEnd
    {
        // 검색을 시작할 최대 인덱스 계산
        const int32 MaxStartIndex = (StartPosition == INDEX_NONE)
                               ? (StrLen - SubStrLen) // 지정 안되면 마지막 가능한 위치부터
                               : FMath::Min(StartPosition, StrLen - SubStrLen); // 지정되면 그 위치까지만

        // 시작 위치가 음수면 검색 불가
        if (MaxStartIndex < 0)
        {
             return INDEX_NONE;
        }

        // 검색 범위: [MaxStartIndex, -1) 즉, MaxStartIndex 부터 0 까지 역방향 검색
        return FindSubString(MaxStartIndex, -1, -1);
    }
}

int32 FString::FindChar(
    ElementType CharToFind, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir, int32 StartPosition
) const
{
    const int32 MyLen = Len();
    if (MyLen == 0)
    {
        return INDEX_NONE;
    }

    // Normalize StartPosition
    StartPosition = (StartPosition < 0) ? 0 : FMath::Min(StartPosition, MyLen - 1);

    ElementType CharToFindLower = (SearchCase == ESearchCase::IgnoreCase) ? FCString::ToLower(CharToFind) : CharToFind;

    if (SearchDir == ESearchDir::FromStart)
    {
        for (int32 i = StartPosition; i < MyLen; ++i)
        {
            ElementType CurrentChar = PrivateString[i];
            ElementType CurrentCharCompare = (SearchCase == ESearchCase::IgnoreCase) ? FCString::ToLower(CurrentChar) : CurrentChar;
            if (CurrentCharCompare == CharToFindLower)
            {
                return i;
            }
        }
    }
    else // ESearchDir::FromEnd
    {
        // Adjust StartPosition for reverse search if it wasn't explicitly set for the end
        // If StartPosition was default (0), start from the actual end.
        // If StartPosition was set, use it as the starting point for reverse search.
        int32 ActualStartPosition = (StartPosition == 0 && SearchDir == ESearchDir::FromEnd) ? MyLen - 1 : StartPosition;
        ActualStartPosition = FMath::Min(ActualStartPosition, MyLen - 1); // Ensure it's within bounds

        for (int32 i = ActualStartPosition; i >= 0; --i)
        {
            ElementType CurrentChar = PrivateString[i];
            ElementType CurrentCharCompare = (SearchCase == ESearchCase::IgnoreCase) ? FCString::ToLower(CurrentChar) : CurrentChar;
            if (CurrentCharCompare == CharToFindLower)
            {
                return i;
            }
        }
    }

    return INDEX_NONE;
}

bool FString::FindChar(ElementType InChar, int32& Index) const
{
    const int32 IndexFound = FindChar(InChar);
    if (IndexFound != INDEX_NONE)
    {
        Index = IndexFound;
        return true;
    }
    return false;
}

void FString::Reserve(int32 CharacterCount)
{
    PrivateString.reserve(CharacterCount);
}

void FString::Resize(int32 CharacterCount)
{
    PrivateString.resize(CharacterCount);
}

FString FString::ToUpper() const &
{
    BaseStringType UpperCaseString = PrivateString;
    std::ranges::transform(
        UpperCaseString,
        UpperCaseString.begin(),
        [](ElementType Char) { return FCString::ToUpper(Char); }
    );
    return FString{std::move(UpperCaseString)};
}

FString FString::ToUpper() &&
{
    std::ranges::transform(
        PrivateString,
        PrivateString.begin(),
        [](ElementType Char) { return FCString::ToUpper(Char); }
    );
    return std::move(*this);
}

void FString::ToUpperInline()
{
    std::ranges::transform(
        PrivateString,
        PrivateString.begin(),
        [](ElementType Char) { return FCString::ToUpper(Char); }
    );
}

FString FString::ToLower() const &
{
    BaseStringType LowerCaseString = PrivateString;
    std::ranges::transform(
        LowerCaseString,
        LowerCaseString.begin(),
        [](ElementType Char) { return FCString::ToLower(Char); }
    );
    return FString{std::move(LowerCaseString)};
}

FString FString::ToLower() &&
{
    std::ranges::transform(
        PrivateString,
        PrivateString.begin(),
        [](ElementType Char) { return FCString::ToLower(Char); }
    );
    return std::move(*this);
}

void FString::ToLowerInline()
{
    std::ranges::transform(
        PrivateString,
        PrivateString.begin(),
        [](ElementType Char) { return FCString::ToLower(Char); }
    );
}

FString FString::Mid(int32 Start, int32 Count) const
{
    const int32 MyLen = Len();
    // Clamp Start to be within the bounds of the string
    Start = FMath::Clamp(Start, 0, MyLen);

    // If Count is negative or too large, adjust it to go up to the end of the string
    if (Count < 0 || (Start + Count) > MyLen)
    {
        Count = MyLen - Start;
    }

    // If the adjusted Count is zero or negative, return an empty string
    if (Count <= 0)
    {
        return FString{};
    }

    // Use std::basic_string::substr
    // substr(pos, count)
    BaseStringType Sub = PrivateString.substr(Start, Count);
    return FString{std::move(Sub)};
}

FString FString::Left(int32 Count) const
{
    const int32 MyLen = Len();
    // Clamp Count to be non-negative and not exceed the string length
    Count = FMath::Clamp(Count, 0, MyLen);

    if (Count == 0)
    {
        return FString{};
    }

    // Use std::basic_string::substr
    // substr(pos, count) - starting from pos 0
    BaseStringType Sub = PrivateString.substr(0, Count);
    return FString{std::move(Sub)};
}

bool FString::RemoveFromStart(const FString& InPrefix, ESearchCase::Type SearchCase)
{
    const int32 PrefixLen = InPrefix.Len();
    const int32 MyLen = Len();

    if (PrefixLen == 0 || PrefixLen > MyLen)
    {
        return false; // Cannot remove an empty or longer prefix
    }

    // Check if the string actually starts with the prefix
    bool bStartsWithPrefix;
    if (SearchCase == ESearchCase::CaseSensitive)
    {
        // Use std::basic_string::compare for case-sensitive comparison
        bStartsWithPrefix = (PrivateString.compare(0, PrefixLen, *InPrefix) == 0);
    }
    else // ESearchCase::IgnoreCase
    {
        // Perform case-insensitive comparison manually or using ranges::equal
        bStartsWithPrefix = std::ranges::equal(
            PrivateString.begin(), PrivateString.begin() + PrefixLen,
            InPrefix.PrivateString.begin(), InPrefix.PrivateString.end(),
            [](ElementType a, ElementType b) { return FCString::ToLower(a) == FCString::ToLower(b); }
        );
    }

    if (bStartsWithPrefix)
    {
        // If it starts with the prefix, remove it using erase or assign with substr
        // erase(pos, count)
        PrivateString.erase(0, PrefixLen);
        return true;
    }

    return false; // Prefix not found at the start
}

// Printf 함수 구현
FString FString::Printf(const ElementType* Format, ...)
{
    if (!Format)
    {
        return FString{};
    }

    va_list ArgPtr;
    int32 Result = -1;
    std::vector<ElementType> Buffer; // 동적 버퍼 사용

    // 필요한 버퍼 크기를 추정하며 반복
    int32 BufferSize = 512; // 초기 시도 크기
    while (true)
    {
        Buffer.resize(BufferSize); // 버퍼 크기 조정

        va_start(ArgPtr, Format); // 각 시도마다 va_list 재시작 필요
#if USE_WIDECHAR
    #ifdef _WIN32
        // _vsnwprintf는 널 종료를 보장하지 않을 수 있으며, 성공 시 문자 수(널 제외) 또는 버퍼가 작으면 -1 반환
        // _TRUNCATE 플래그를 사용하면 버퍼에 맞게 잘라주고 널 종료를 보장하며 성공 시 문자 수(널 미포함), 잘렸으면 -1 반환
        Result = _vsnwprintf_s(Buffer.data(), Buffer.size(), _TRUNCATE, Format, ArgPtr);
        // _vsnwprintf_s는 성공해도 버퍼가 꽉 찼으면 -1 반환 가능. 필요한 크기를 알려주지 않음.
        // 따라서 아래 C99 표준 vsnprintf/vswprintf 방식 사용이 더 효과적일 수 있음 (Windows에서도 사용 가능).
        // 여기서는 일단 C99 표준 함수를 우선 사용하도록 수정
    #endif
    // C99 vswprintf (대부분의 현대 컴파일러에서 지원, Windows 포함)
    // 성공 시: 필요한 문자 수 (널 제외) 반환. 버퍼 크기보다 작으면 버퍼에 쓰여짐.
    // 버퍼 부족 시: 필요한 문자 수 (널 제외) 반환. 버퍼 내용은 미정의.
    // 오류 시: 음수 반환.
    Result = vswprintf(Buffer.data(), Buffer.size(), Format, ArgPtr);

#else // TCHAR == char
        // C99 vsnprintf
        // 성공 시: 필요한 문자 수 (널 제외) 반환. 버퍼 크기보다 작으면 버퍼에 쓰여짐.
        // 버퍼 부족 시: 필요한 문자 수 (널 제외) 반환. 버퍼 내용은 미정의.
        // 오류 시: 음수 반환.
        Result = vsnprintf(Buffer.data(), Buffer.size(), Format, ArgPtr);
#endif
        va_end(ArgPtr);

        // 결과 확인
        if (Result < 0)
        {
            // 포맷팅 오류 발생
            // 오류 로그 출력 가능
            return FString{}; // 빈 문자열 반환
        }
        else if (Result < BufferSize)
        {
            // 성공: 버퍼가 충분했음. Result는 쓰여진 문자 수 (널 제외).
            // Buffer.data()는 이미 널 종료된 상태(vsnprintf/vswprintf가 보장).
            // 필요한 경우 Buffer.resize(Result); 로 정확한 크기로 줄일 수 있음.
            return FString{BaseStringType(Buffer.data())};
        }
        else // Result >= BufferSize
        {
            // 버퍼 부족: Result는 필요한 버퍼 크기 (널 포함).
            // 필요한 크기 + 1 (널 문자 공간)으로 버퍼 재할당 후 재시도.
            BufferSize = Result + 1;
            // 무한 루프 방지 (매우 큰 문자열 등) - 필요시 최대 크기 제한 추가 가능
            // if (BufferSize > SOME_MAX_LIMIT) { return FString{}; }
        }
    }
}

FString operator/(const FString& Lhs, const FString& Rhs)
{
    // Lhs가 비어있으면 Rhs만 반환 (Rhs가 절대 경로일 수 있음)
    if (Lhs.IsEmpty())
    {
        return Rhs;
    }

    // Rhs가 비어있으면 Lhs만 반환
    if (Rhs.IsEmpty())
    {
        return Lhs;
    }

    FString Result = Lhs; // Lhs 복사

    // Lhs의 마지막 문자가 경로 구분자인지 확인
    bool bLhsEndsWithSlash = false;
    if (Result.Len() > 0)
    {
        const FString::ElementType LastChar = Result[Result.Len() - 1];
        if (LastChar == TEXT('/') || LastChar == TEXT('\\')) // 두 종류의 구분자 모두 고려
        {
            bLhsEndsWithSlash = true;
        }
    }

    // Rhs의 첫 번째 문자가 경로 구분자인지 확인
    bool bRhsStartsWithSlash = false;
    if (Rhs.Len() > 0)
    {
        const FString::ElementType FirstChar = Rhs[0];
        if (FirstChar == TEXT('/') || FirstChar == TEXT('\\'))
        {
            bRhsStartsWithSlash = true;
        }
    }

    // 경로 구분자 처리 로직
    if (bLhsEndsWithSlash && bRhsStartsWithSlash)
    {
        // 예: "Dir/" / "/File.txt" -> "Dir/File.txt"
        // Rhs에서 첫 번째 슬래시 제거 후 합침
        Result += Rhs.Mid(1); // Mid(1)은 두 번째 문자부터 끝까지
    }
    else if (!bLhsEndsWithSlash && !bRhsStartsWithSlash)
    {
        // 예: "Dir" / "File.txt" -> "Dir/File.txt"
        // 중간에 슬래시 추가 후 합침
        Result += TEXT("/"); // 표준 경로 구분자로 '/' 사용 권장
        Result += Rhs;
    }
    else // (bLhsEndsWithSlash && !bRhsStartsWithSlash) 또는 (!bLhsEndsWithSlash && bRhsStartsWithSlash)
    {
        // 예: "Dir/" / "File.txt" -> "Dir/File.txt"
        // 예: "Dir" / "/File.txt" -> "Dir/File.txt"
        // 그냥 합침
        Result += Rhs;
    }

    return Result;
}

// FString과 C-스타일 문자열을 위한 오버로딩
FString operator/(const FString& Lhs, const FString::ElementType* Rhs)
{
    if (Rhs == nullptr || *Rhs == TEXT('\0')) // Rhs가 비어있는지 확인
    {
        return Lhs;
    }
    return Lhs / FString(Rhs); // FString으로 변환 후 위임
}

FString operator/(const FString::ElementType* Lhs, const FString& Rhs)
{
    if (Lhs == nullptr || *Lhs == TEXT('\0')) // Lhs가 비어있는지 확인
    {
        return Rhs;
    }
    return FString(Lhs) / Rhs; // FString으로 변환 후 위임
}
