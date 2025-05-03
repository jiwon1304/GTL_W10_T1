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


bool FDesktopPlatformWindows::OpenFileDialog(
    const FString& DialogTitle,
    const FString& DefaultPath,
    const TArray<FFilterItem>& FileTypes,
    EFileDialogFlag Flag,
    TArray<FString>& OutFilenames
) {
    const void* ParentWindowHandle = nullptr;

    fs::path Directory = DefaultPath.ToWideString();
    Directory.remove_filename();

    return FileDialogShared(
        false,
        ParentWindowHandle,
        DialogTitle,
        Directory.generic_wstring(),
        TEXT(""),
        FileTypes,
        Flag,
        OutFilenames
    );
}

bool FDesktopPlatformWindows::SaveFileDialog(
    const FString& DialogTitle,
    const FString& DefaultPathAndFileName,
    const TArray<FFilterItem>& FileTypes,
    TArray<FString>& OutFilenames
) {
    const void* ParentWindowHandle = nullptr;

    fs::path Directory = DefaultPathAndFileName.ToWideString();
    const fs::path FileName = Directory.filename();
    Directory.remove_filename();

    return FileDialogShared(
        true,
        ParentWindowHandle,
        DialogTitle,
        Directory.generic_wstring(),
        FileName.generic_wstring(),
        FileTypes,
        EFileDialogFlag::None,
        OutFilenames
    );
}

bool FDesktopPlatformWindows::SaveFileDialog(
    const FString& DialogTitle,
    const FString& DefaultPath,
    const FString& DefaultFile,
    const TArray<FFilterItem>& FileTypes,
    TArray<FString>& OutFilenames
) {
    const void* ParentWindowHandle = nullptr;

    fs::path Directory = DefaultPath.ToWideString();
    Directory.remove_filename();
    const fs::path FileName = DefaultFile.ToWideString();

    return FileDialogShared(
        true,
        ParentWindowHandle,
        DialogTitle,
        Directory.generic_wstring(),
        FileName.generic_wstring(),
        FileTypes,
        EFileDialogFlag::None,
        OutFilenames
    );
}

bool FDesktopPlatformWindows::FileDialogShared(
    bool bSave,
    const void* ParentWindowHandle,
    const FString& DialogTitle,
    const FString& DefaultPath,
    const FString& DefaultFile,
    const TArray<FFilterItem>& FileTypes,
    EFileDialogFlag Flag,
    TArray<FString>& OutFilenames
) {
    bool bSuccess = false;

    // IFileDialog 인스턴스 생성
    TComPtr<IFileDialog> FileDialog;
    HRESULT DialogResult = ::CoCreateInstance(
        bSave ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_ALL,
        bSave ? IID_IFileSaveDialog : IID_IFileOpenDialog,
        reinterpret_cast<void**>(&FileDialog)
    );

    // 생성 성공 및 포인터 유효 확인
    if (SUCCEEDED(DialogResult) && FileDialog)
    {
        // 다이얼로그 옵션 설정
        DWORD DialogOptions = 0;
        if (SUCCEEDED(FileDialog->GetOptions(&DialogOptions)))
        {
            if (bSave)
            {
                // FOS_OVERWRITEPROMPT: 같은 이름의 파일이 있을 경우 덮어쓸지 묻습니다.
                // FOS_PATHMUSTEXIST: 경로가 존재해야 합니다.
                // FOS_FORCEFILESYSTEM: 파일 시스템 경로만 허용합니다.
                FileDialog->SetOptions(DialogOptions | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
            }
            else
            {
                if (SUCCEEDED(FileDialog->GetOptions(&DialogOptions)))
                {
                    if (Flag == EFileDialogFlag::Multiple)
                    {
                        DialogOptions |= FOS_ALLOWMULTISELECT;
                    }
                    FileDialog->SetOptions(DialogOptions | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
                }
            }
        }

        // 필터 설정
        TArray<COMDLG_FILTERSPEC> ComFilterSpecs;
        TArray<std::wstring> WideFilterStrings;
        ComFilterSpecs.Reserve(FileTypes.Num());
        WideFilterStrings.Reserve(FileTypes.Num() * 2);

        for (const auto& [FilterPattern, Description] : FileTypes)
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
            FileDialog->SetFileTypes(static_cast<UINT>(ComFilterSpecs.Num()), ComFilterSpecs.GetData());
            FileDialog->SetFileTypeIndex(1);
        }

        // 타이틀 설정
        if (!DialogTitle.IsEmpty())
        {
            FileDialog->SetTitle(DialogTitle.ToWideString().c_str());
        }

        // 기본 경로 및 파일 이름 설정
        if (bSave && !DefaultFile.IsEmpty())
        {
            const fs::path DefaultFileName = DefaultFile.ToWideString();
            FileDialog->SetFileName(DefaultFileName.filename().c_str());
        }

        if (!DefaultPath.IsEmpty())
        {
            const fs::path DefaultDirectory = DefaultPath.ToWideString();
            std::error_code ErrorCode;
            if (
                fs::exists(DefaultDirectory, ErrorCode)
                && ErrorCode == std::error_code{}
                && fs::is_directory(DefaultDirectory, ErrorCode)
                && ErrorCode == std::error_code{}
            ) {
                TComPtr<IShellItem> DefaultFolderItem;
                DialogResult = ::SHCreateItemFromParsingName(DefaultDirectory.c_str(), nullptr, IID_PPV_ARGS(&DefaultFolderItem));
                if (SUCCEEDED(DialogResult) && DefaultFolderItem)
                {
                    FileDialog->SetFolder(DefaultFolderItem.Get());
                }
            }
        }
    }

    // 다이얼로그 표시
    DialogResult = FileDialog->Show((HWND)ParentWindowHandle);
    if (SUCCEEDED(DialogResult))
    {
        if (bSave)
        {
            TComPtr<IShellItem> Result;
            DialogResult = FileDialog->GetResult(&Result);
            if (SUCCEEDED(DialogResult) && Result)
            {
                PWSTR FilePathPtr = nullptr;
                DialogResult = Result->GetDisplayName(SIGDN_FILESYSPATH, &FilePathPtr);

                // 선택된 파일 경로 가져오기
                if (SUCCEEDED(DialogResult))
                {
                    bSuccess = true;
                    OutFilenames.Add(fs::path(FilePathPtr).generic_wstring()); // PWSTR -> FString 변환 필요
                    ::CoTaskMemFree(FilePathPtr);
                }
            }
        }
        else
        {
            IFileOpenDialog* FileOpenDialog = reinterpret_cast<IFileOpenDialog*>(FileDialog.Get());
            TComPtr<IShellItemArray> Results;
            if (SUCCEEDED(FileOpenDialog->GetResults(&Results)))
            {
                DWORD NumResults = 0;
                Results->GetCount(&NumResults);
                for (DWORD ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
                {
                    TComPtr<IShellItem> SelectedItem;
                    if (SUCCEEDED(Results->GetItemAt(ResultIndex, &SelectedItem)))
                    {
                        PWSTR pFilePath = nullptr;
                        if (SUCCEEDED(SelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath)))
                        {
                            // 선택된 파일 경로 가져오기
                            bSuccess = true;
                            OutFilenames.Add(fs::path(pFilePath).generic_wstring());
                            ::CoTaskMemFree(pFilePath);
                        }
                    }
                }
            }
        }
    }
    ::CoUninitialize();
    return bSuccess;
}
