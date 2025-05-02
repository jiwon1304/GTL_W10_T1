// ReSharper disable CppClangTidyClangDiagnosticCastQual
// ReSharper disable CppCStyleCast
#include "WindowsFileDialog.h"
#include <cassert>
#include <filesystem>    // std::filesystem
#include <ShObjIdl.h>    // IFileOpenDialog, IFileSaveDialog
#include <string>        // std::wstring
#include <system_error>  // std::error_code for filesystem checks
#include <utility>       // std::swap, std::forward
#include <Windows.h>

#include "UserInterface/Console.h"

// 네임스페이스 별칭
namespace fs = std::filesystem;

// --- 여기서만 사용하는 간단한 COM 스마트 포인터 ---
template <typename T>
class TComPtr
{
private:
    T* Ptr = nullptr;

public:
    // 기본 생성자 (nullptr 초기화)
    TComPtr() = default;

    // nullptr로부터의 생성 (명시적)
    explicit TComPtr(std::nullptr_t)
        : Ptr(nullptr)
    {
    }

    // 소멸자: 유효한 포인터면 Release 호출
    ~TComPtr()
    {
        if (Ptr)
        {
            Ptr->Release();
        }
    }

    // 복사 생성자 삭제 (소유권 복제 방지)
    TComPtr(const TComPtr&) = delete;
    // 복사 대입 연산자 삭제
    TComPtr& operator=(const TComPtr&) = delete;

    // 이동 생성자
    TComPtr(TComPtr&& Other) noexcept
        : Ptr(Other.Ptr)
    {
        Other.Ptr = nullptr; // 원본은 소유권 잃음
    }

    // 이동 대입 연산자
    TComPtr& operator=(TComPtr&& Other) noexcept
    {
        if (this != &Other)
        {
            if (Ptr)
            {
                // 기존 리소스 해제
                Ptr->Release();
            }
            Ptr = Other.Ptr;     // 소유권 이전
            Other.Ptr = nullptr; // 원본은 소유권 잃음
        }
        return *this;
    }

    // 원시 포인터 반환
    FORCEINLINE T* Get() const
    {
        assert(Ptr != nullptr);
        return Ptr;
    }

    // 멤버 접근 연산자
    T* operator->() const
    {
        return Get();
    }

    // CoCreateInstance, GetResult 등에서 주소(&)를 사용하기 위한 연산자
    // 주의: 호출 전에 기존 포인터는 Release됨
    T** operator&()  // NOLINT(google-runtime-operator)
    {
        Release(); // 기존 포인터 해제 후 주소 반환 준비
        return &Ptr;
    }

    // T**를 받는 함수의 인자로 사용될 때 (예: CoCreateInstance)
    // void** 로 변환하기 위한 캐스팅 연산자 또는 함수 필요 시 추가 가능
    // 예: operator void**() { return reinterpret_cast<void**>(operator&()); } - 사용 시 주의

    // 명시적 bool 변환 (포인터 유효성 검사)
    explicit operator bool() const
    {
        return Ptr != nullptr;
    }

    // 현재 포인터를 Release하고 nullptr로 설정
    void Release()
    {
        if (Ptr)
        {
            T* Temp = Ptr;
            Ptr = nullptr; // Release 중 재진입 방지
            Temp->Release();
        }
    }

    // 현재 포인터를 교체 (기존 포인터는 Release)
    void Reset(T* NewPtr = nullptr)
    {
        if (Ptr != NewPtr)
        {
            Release();
            Ptr = NewPtr;
        }
    }

    // 소유권을 포기하고 원시 포인터 반환 (Release 호출 안 함)
    T* Detach()
    {
        T* Temp = Ptr;
        Ptr = nullptr;
        return Temp;
    }
};


FString FDesktopPlatformWindows::OpenFileDialog(
    const void* ParentWindowHandle,
    const FString& Title,
    const FString& DefaultPathAndFileName,
    const TArray<FFilterItem>& Filters
) {
    FString SelectedFilePath = TEXT("");

    // IFileOpenDialog 인스턴스 생성
    TComPtr<IFileOpenDialog> FileOpenDialog;
    HRESULT Result = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_ALL,
        IID_IFileOpenDialog,
        reinterpret_cast<void**>(&FileOpenDialog)
    );

    // 생성 성공 및 포인터 유효 확인
    if (SUCCEEDED(Result) && FileOpenDialog)
    {
        // 다이얼로그 옵션 설정
        DWORD DialogOptions = 0;
        if (SUCCEEDED(FileOpenDialog->GetOptions(&DialogOptions)))
        {
            FileOpenDialog->SetOptions(DialogOptions | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        }

        // 필터 설정
        TArray<COMDLG_FILTERSPEC> ComFilterSpecs;
        TArray<std::wstring> WideFilterStrings;
        ComFilterSpecs.Reserve(Filters.Num());
        WideFilterStrings.Reserve(Filters.Num() * 2);

        for (const auto& [FilterPattern, Description] : Filters)
        {
            const int32 DescIndex = WideFilterStrings.Emplace(Description.ToWideString());
            const int32 FilterIndex = WideFilterStrings.Emplace(FilterPattern.ToWideString());
            ComFilterSpecs.Add({
                .pszName = WideFilterStrings[DescIndex].c_str(),
                .pszSpec = WideFilterStrings[FilterIndex].c_str()
            });
        }
        if (ComFilterSpecs.Num() > 0)
        {
            FileOpenDialog->SetFileTypes(static_cast<UINT>(ComFilterSpecs.Num()), ComFilterSpecs.GetData());
        }

        // 타이틀 설정
        if (!Title.IsEmpty())
        {
            FileOpenDialog->SetTitle(Title.ToWideString().c_str());
        }

        // 기본 경로 및 파일 이름 설정
        if (!DefaultPathAndFileName.IsEmpty())
        {
            try
            {
                const fs::path DefaultInputPath = DefaultPathAndFileName.ToWideString();
                fs::path DefaultDirectory;

                // 입력 경로가 디렉토리인지 파일인지 확인하여 기본 폴더 결정
                std::error_code ErrorCode;
                if (fs::is_directory(DefaultInputPath, ErrorCode) && ErrorCode == std::error_code{})
                {
                    DefaultDirectory = DefaultInputPath;
                }
                else if (DefaultInputPath.has_parent_path())
                {
                    DefaultDirectory = DefaultInputPath.parent_path();
                    // 파일 이름도 설정하려면 아래 로직 사용 가능 (Open에서는 보통 안 함)
                    // if (DefaultInputPath.has_filename()) {
                    //     FileOpenDialog->SetFileName(DefaultInputPath.filename().c_str());
                    // }
                }

                // 기본 폴더 설정 시도 (존재하는 경우에만)
                if (
                    !DefaultDirectory.empty()
                    && fs::exists(DefaultDirectory, ErrorCode)
                    && ErrorCode == std::error_code{}
                    && fs::is_directory(DefaultDirectory, ErrorCode)
                    && ErrorCode == std::error_code{}
                ) {
                    TComPtr<IShellItem> DefaultFolderItem;
                    Result = SHCreateItemFromParsingName(DefaultDirectory.c_str(), nullptr, IID_PPV_ARGS(&DefaultFolderItem));
                    if (SUCCEEDED(Result) && DefaultFolderItem)
                    {
                        FileOpenDialog->SetFolder(DefaultFolderItem.Get());
                    }
                }
            }
            catch (const fs::filesystem_error& Error)
            {
                // 파일 시스템 오류 로깅 (예: 잘못된 경로 형식)
                UE_LOG(ELogLevel::Error, "Filesystem error processing default path: %hs", Error.what());
            }
        }

        Result = FileOpenDialog->Show((HWND)ParentWindowHandle);
        if (SUCCEEDED(Result)) // 사용자가 '열기'를 누름 (취소 아님)
        {
            TComPtr<IShellItem> SelectedItem;
            Result = FileOpenDialog->GetResult(&SelectedItem);
            if (SUCCEEDED(Result) && SelectedItem)
            {
                PWSTR FilePathPtr = nullptr;
                Result = SelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &FilePathPtr);

                // 선택된 파일 경로 가져오기
                if (SUCCEEDED(Result))
                {
                    SelectedFilePath = FString(FilePathPtr); // PWSTR -> FString 변환 필요
                    CoTaskMemFree(FilePathPtr);
                }
            }
        }
    }
    CoUninitialize();
    return SelectedFilePath;
}


FString FDesktopPlatformWindows::SaveFileDialog(
    const void* ParentWindowHandle,
    const FString& Title,
    const FString& DefaultPathAndFileName,
    const TArray<FFilterItem>& Filters
) {
    FString SelectedFilePath = TEXT("");

    TComPtr<IFileSaveDialog> FileSaveDialog;
    HRESULT Result = CoCreateInstance(
        CLSID_FileSaveDialog,
        nullptr,
        CLSCTX_ALL,
        IID_IFileSaveDialog,
        reinterpret_cast<void**>(&FileSaveDialog)
    );

    if (SUCCEEDED(Result) && FileSaveDialog)
    {
        // 다이얼로그 옵션 설정
        DWORD DialogOptions = 0;
        if (SUCCEEDED(FileSaveDialog->GetOptions(&DialogOptions)))
        {
            // FOS_OVERWRITEPROMPT: 같은 이름의 파일이 있을 경우 덮어쓸지 묻습니다.
            // FOS_PATHMUSTEXIST: 경로가 존재해야 합니다.
            // FOS_FORCEFILESYSTEM: 파일 시스템 경로만 허용합니다.
            FileSaveDialog->SetOptions(DialogOptions | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        }

        // 필터 설정
        TArray<COMDLG_FILTERSPEC> ComFilterSpecs;
        TArray<std::wstring> WideFilterStrings;
        ComFilterSpecs.Reserve(Filters.Num());
        WideFilterStrings.Reserve(Filters.Num() * 2);

        for (const auto& [FilterPattern, Description] : Filters)
        {
            const int32 DescIndex = WideFilterStrings.Emplace(Description.ToWideString());
            const int32 FilterIndex = WideFilterStrings.Emplace(FilterPattern.ToWideString());
            ComFilterSpecs.Add({
                .pszName = WideFilterStrings[DescIndex].c_str(),
                .pszSpec = WideFilterStrings[FilterIndex].c_str()
            });
        }

        if (ComFilterSpecs.Num() > 0)
        {
            FileSaveDialog->SetFileTypes(static_cast<UINT>(ComFilterSpecs.Num()), ComFilterSpecs.GetData());
            // 기본 필터 인덱스 설정 (예: 첫 번째 필터)
            FileSaveDialog->SetFileTypeIndex(1); // 인덱스는 1부터 시작
        }

        // 타이틀 설정
        if (!Title.IsEmpty())
        {
            FileSaveDialog->SetTitle(Title.ToWideString().c_str());
        }

        // 기본 경로 및 파일 이름 설정
        try
        {
            const fs::path DefaultInputPath = DefaultPathAndFileName.ToWideString();
            fs::path DefaultDirectory;
            fs::path DefaultFileName;

            // 입력 경로에서 디렉토리와 파일명 분리
            if (DefaultInputPath.has_parent_path())
            {
                DefaultDirectory = DefaultInputPath.parent_path();
            }
            if (DefaultInputPath.has_filename())
            {
                DefaultFileName = DefaultInputPath.filename();
            }

            // 기본 폴더 설정 (존재하는 경우)
            std::error_code ErrorCode;
            if (
                !DefaultDirectory.empty()
                && fs::exists(DefaultDirectory, ErrorCode)
                && ErrorCode == std::error_code{}
                && fs::is_directory(DefaultDirectory, ErrorCode)
                && ErrorCode == std::error_code{}
            ) {
                TComPtr<IShellItem> DefaultFolderItem;
                Result = SHCreateItemFromParsingName(DefaultDirectory.c_str(), nullptr, IID_PPV_ARGS(&DefaultFolderItem));
                if (SUCCEEDED(Result) && DefaultFolderItem)
                {
                    FileSaveDialog->SetFolder(DefaultFolderItem.Get());
                }
            }

            // 기본 파일 이름 설정
            if (!DefaultFileName.empty())
            {
                FileSaveDialog->SetFileName(DefaultFileName.c_str());
            }

            // 기본 확장자 설정 (std::filesystem::path::extension 사용 가능)
            // 또는 기존 필터 기반 로직 사용
            fs::path FileNameForExt = DefaultFileName.empty() ? fs::path(TEXT("file")) : DefaultFileName; // 파일명이 없으면 임시 이름 사용
            if (!FileNameForExt.has_extension() && Filters.Num() > 0 && !Filters[0].FilterPattern.IsEmpty())
            {
                // 기존 로직 활용 (첫 필터에서 확장자 추출)
                FString FilterPattern = Filters[0].FilterPattern;
                int32 WildcardIndex = FilterPattern.Find(TEXT("*."), ESearchCase::CaseSensitive); // "*." 위치 찾기
                if (WildcardIndex != INDEX_NONE)
                {
                    FString ExtensionPart = FilterPattern.Mid(WildcardIndex + 2); // "*." 다음부터
                    int32 SemicolonIndex = ExtensionPart.Find(TEXT(";"));
                    if (SemicolonIndex != INDEX_NONE)
                    {
                        ExtensionPart = ExtensionPart.Left(SemicolonIndex); // 첫 번째 확장자만
                    }
                    if (!ExtensionPart.IsEmpty())
                    {
                        FileSaveDialog->SetDefaultExtension(ExtensionPart.ToWideString().c_str());
                    }
                }
            }
        }
        catch (const fs::filesystem_error& Error)
        {
            // 파일 시스템 오류 로깅
            UE_LOG(ELogLevel::Error, "Filesystem error processing default path/file: %hs", Error.what());
        }

        // 다이얼로그 표시
        Result = FileSaveDialog->Show((HWND)ParentWindowHandle);
        if (SUCCEEDED(Result)) // 사용자가 '저장'을 누름
        {
            TComPtr<IShellItem> SelectedItem;
            Result = FileSaveDialog->GetResult(&SelectedItem);
            if (SUCCEEDED(Result) && SelectedItem)
            {
                PWSTR FilePathPtr = nullptr;
                Result = SelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &FilePathPtr);

                // 선택된 파일 경로 가져오기
                if (SUCCEEDED(Result))
                {
                    SelectedFilePath = FString(FilePathPtr); // PWSTR -> FString 변환 필요
                    CoTaskMemFree(FilePathPtr);
                }
            }
        }
    }
    CoUninitialize();
    return SelectedFilePath;
}
