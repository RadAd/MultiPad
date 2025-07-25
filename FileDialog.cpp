#include <Windows.h>
#include <atlbase.h>
#include <ShObjIdl_core.h>
#include <ShObjIdl.h>
#include <tchar.h>
#include <string>
#include "Rad/WinError.h"

#define ID_ENCODING 100

const COMDLG_FILTERSPEC c_rgSaveTypes[] =
{
    {L"Text Document (*.txt)",       L"*.txt"},
    {L"All Documents (*.*)",         L"*.*"}
};

// https://learn.microsoft.com/en-us/windows/win32/shell/common-file-dialog
// https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winui/shell/appplatform/commonfiledialog/CommonFileDialogApp.cpp

bool FileOpenDialog(HWND hWndOwner, std::wstring& filename, DWORD& cp)
{
    CComPtr<IFileOpenDialog> pDlg;
    CHECK_HR(pDlg.CoCreateInstance(CLSID_FileOpenDialog));

    CComQIPtr<IFileDialogCustomize> pCustomize(pDlg);
    if (pCustomize)
    {
#if 0
        CHECK_HR(pCustomize->AddEditBox(ID_ENCODING, TEXT("0")));
        CHECK_HR(pCustomize->SetControlLabel(ID_ENCODING, TEXT("Encoding"))); // TODO Doesn't seem to appear
#else
        CHECK_HR(pCustomize->AddComboBox(ID_ENCODING));
        CHECK_HR(pCustomize->SetControlLabel(ID_ENCODING, TEXT("Encoding"))); // TODO Doesn't seem to appear
        CHECK_HR(pCustomize->AddControlItem(ID_ENCODING, CP_ACP, TEXT("Auto Detect")));
        CHECK_HR(pCustomize->AddControlItem(ID_ENCODING, CP_UTF8, TEXT("UTF-8")));   // TODO Add some encoding options
        CHECK_HR(pCustomize->AddControlItem(ID_ENCODING, 936, TEXT("Chinese (Simplified)")));
        CHECK_HR(pCustomize->SetSelectedControlItem(ID_ENCODING, CP_ACP));
#endif
    }

    FILEOPENDIALOGOPTIONS dwFlags = {};
    CHECK_HR(pDlg->GetOptions(&dwFlags));
    dwFlags |= FOS_FORCEFILESYSTEM;
    dwFlags |= FOS_FILEMUSTEXIST;
    CHECK_HR(pDlg->SetOptions(dwFlags));

    CHECK_HR(pDlg->SetFileTypes(ARRAYSIZE(c_rgSaveTypes), c_rgSaveTypes));
    CHECK_HR(pDlg->SetFileTypeIndex(1));
    CHECK_HR(pDlg->SetDefaultExtension(L"txt"));

    if (SUCCEEDED(pDlg->Show(hWndOwner)))
    {
        CComPtr <IShellItem> psiResult;
        CHECK_HR(pDlg->GetResult(&psiResult));
        PWSTR pszFilePath = NULL;
        CHECK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));
        if (pszFilePath)
            filename = pszFilePath;
        CoTaskMemFree(pszFilePath);

        cp = CP_ACP;
        if (pCustomize)
        {
#if 0
            PWSTR pszText = NULL;
            CHECK_HR(pCustomize->GetEditBoxText(ID_ENCODING, &pszText));
            if (pszText)
                cp = _tcstol(pszText, nullptr, 10);
            CoTaskMemFree(pszText);
#else
            CHECK_HR(pCustomize->GetSelectedControlItem(ID_ENCODING, &cp));
#endif
        }
        return true;
    }
    else
        return false;
}
