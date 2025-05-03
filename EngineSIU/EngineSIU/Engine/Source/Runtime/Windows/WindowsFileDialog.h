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

enum class EFileDialogFlag : uint8
{
    None,    // No flags
    Multiple // Allow multiple file selections
};

struct FDesktopPlatformWindows
{
public:
    /** 파일 열기 대화상자를 표시하고 선택된 파일 경로를 반환합니다. */
    static bool OpenFileDialog(
        const FString& DialogTitle,
        const FString& DefaultPath,
        const TArray<FFilterItem>& FileTypes,
        EFileDialogFlag Flag,
        TArray<FString>& OutFilenames
    );

    /** 파일 저장 대화상자를 표시하고 선택된 파일 경로를 반환합니다. */
    static bool SaveFileDialog(
        const FString& DialogTitle,
        const FString& DefaultPathAndFileName,
        const TArray<FFilterItem>& FileTypes,
        TArray<FString>& OutFilenames
    );

    static bool SaveFileDialog(
        const FString& DialogTitle,
        const FString& DefaultPath,
        const FString& DefaultFile,
        const TArray<FFilterItem>& FileTypes,
        TArray<FString>& OutFilenames
    );

private:
    static bool FileDialogShared(
        bool bSave,
        const void* ParentWindowHandle,
        const FString& DialogTitle,
        const FString& DefaultPath,
        const FString& DefaultFile,
        const TArray<FFilterItem>& FileTypes,
        EFileDialogFlag Flag,
        TArray<FString>& OutFilenames
    );
};
