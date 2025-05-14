#pragma once
#include "String.h"
#include "Array.h"
#include "Map.h"

// Add a helper function to parse a string into a TMap<FString, FString>
namespace ContainerHelpers
{
    int32 ParseIntoArray(const FString& Source, TArray<FString>& OutArray, const TCHAR* Separator, bool bCullEmpty)
    {
        OutArray.Empty();

        int32 SepLen = FCString::Strlen(Separator);
        int32 Start = 0;

        while (Start <= Source.Len())
        {
            int32 SepIdx = Source.Find(Separator, ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
            FString Token;

            if (SepIdx == INDEX_NONE)
            {
                Token = Source.Mid(Start);
                Start = Source.Len() + 1;
            }
            else
            {
                Token = Source.Mid(Start, SepIdx - Start);
                Start = SepIdx + SepLen;
            }

            if (!bCullEmpty || !Token.IsEmpty())
            {
                OutArray.Add(Token);
            }
        }

        return OutArray.Num();
    }

    // 가변 길이 컨테이너는 시작, 종료, separator를 사용하여 문자열을 생성합니다.
    FString TMapToString(TMap<K Map) const
    {
        FString Result;
        Result += FString::Printf(TEXT("@@TMAP_START@@"));
        for (const auto& Pair : ContainerPrivate)
        {
            Result += FString::Printf(TEXT("%s: %s@@TMAP_SEP@@"), *Pair.first.ToString(), *Pair.second.ToString());
        }
        Result += FString::Printf(TEXT("@@TMAP_END@@"));
        return Result;
    }

    static bool ParseStringToTMap(const FString& InString, TMap<FString, FString>& OutMap)
    {
        OutMap.Empty();

        const FString StartMarker = TEXT("@@TMAP_START@@");
        const FString EndMarker = TEXT("@@TMAP_END@@");
        const FString PairSeparator = TEXT("@@TMAP_SEP@@");
        const FString KeyValueSeparator = TEXT(": ");

        // 1. 시작/끝 마커 위치 찾기
        int32 StartIdx = InString.Find(StartMarker, ESearchCase::CaseSensitive);
        int32 EndIdx = InString.Find(EndMarker, ESearchCase::CaseSensitive);

        if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE || StartIdx >= EndIdx)
        {
            return false;
        }

        // 2. 마커 사이의 내용 추출
        int32 ContentStart = StartIdx + StartMarker.Len();
        int32 ContentLen = EndIdx - ContentStart;
        FString Content = InString.Mid(ContentStart, ContentLen);

        // 3. PairSeparator(@@TMAP_SEP@@)로 분리
        TArray<FString> Pairs;
        ParseIntoArray(Content, Pairs, *PairSeparator, /*bCullEmpty=*/true);

        for (const FString& Pair : Pairs)
        {
            FString Key, Value;
            // 4. KeyValueSeparator(": ")로 분리
            if (Pair.Split(KeyValueSeparator, &Key, &Value))
            {
                OutMap.Add(Key, Value);
            }
        }

        return true;
    }

    template<typename K, typename V>
    bool InitFromString(const FString& InString)
    {
        const FString& PairSeparator = TEXT("@@TMAP_SEP@@");
        const FString& KeyValueSeparator = TEXT(": ");

        TMap<K, V>& OutMap;
        OutMap.Empty();

        TArray<FString> Pairs;
        Array ParseIntoArray(Pairs, *PairSeparator, true);

        for (const FString& PairString : Pairs)
        {
            FString KeyString, ValueString;
            if (PairString.Split(KeyValueSeparator, &KeyString, &ValueString))
            {
                K Key;
                V Value;
                Key.InitFromString(KeyString);
                Value.InitFromString(ValueString);
                OutMap.Add(Key, Value);
            }
        }
        return true;
    }

}    // Update the problematic line to use the helper function
