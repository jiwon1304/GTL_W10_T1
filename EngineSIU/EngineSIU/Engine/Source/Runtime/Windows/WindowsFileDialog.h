#pragma once
#include "Container/Array.h"
#include "Container/String.h"


/** File Dialog 필터 항목 구조체 정의 */
struct FFilterItem
{
    /** 필터링 할 파일 확장자 ex) *.obj, *.fbx */
    FString FilterPattern;

    /** 필터링 할 파일 확장자에 대한 설명 ex) Wavefront OBJ, FBX */
    FString Description;
};

struct FWindowsFileDialog
{
public:
    /** 파일 열기 대화상자를 표시하고 선택된 파일 경로를 반환합니다. */
    static FString OpenFileDialog(
        const FString& Title,
        const FString& DefaultPathAndFileName,
        const TArray<FFilterItem>& Filter
    );

    /** 파일 저장 대화상자를 표시하고 선택된 파일 경로를 반환합니다. */
    static FString SaveFileDialog(
        const FString& Title,
        const FString& DefaultPathAndFileName,
        const TArray<FFilterItem>& Filter
    );
};
