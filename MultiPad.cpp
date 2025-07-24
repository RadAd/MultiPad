#include "Rad/MDI.h"
#include "Rad/Dialog.h"
#include "Rad/Windowxx.h"
#include "Rad/WinError.h"
#include "Rad/MemoryPlus.h"
#include "Rad/RadTextFile.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <shlwapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <vector>
#include "resource.h"

// TODO
// Application icon
// Document icon
// Toolbar
// Status bar cursor pos/selection
// Warn when closing an unsaved file
// Monitor for file updates
// Multi undo
// Support for bom
// Word boundaries
// Save position in registry
// Choose font
// Word wrap
// Drop file support
// Open from url
// Recent file list
// goto line
// tab mode
// split view
// open from command line

// TODO - Not sure this can be done with the edit control
// Show whitespace
// Show unprintable characters

namespace stdt
{
#ifdef UNICODE
    using string = std::wstring;
    using string_view = std::wstring_view;
#else
    using string = std::string;
    using string_view = std::string_view;
#endif
}

extern HINSTANCE g_hInstance;
extern HACCEL g_hAccelTable;
extern HWND g_hWndAccel;

const TCHAR* g_ProjectName = TEXT("MultiPad");
const TCHAR* g_ProjectTitle = TEXT("MultiPad");

inline stdt::string_view right(stdt::string_view sv, size_t len)
{
    if (sv.size() <= len)
        return sv;
    return sv.substr(sv.size() - len);
}

inline void replaceAll(stdt::string& str, stdt::string_view from, stdt::string_view to)
{
    size_t start_pos = str.find(from);
    while (start_pos != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos = str.find(from, start_pos + to.length());
    }
}

inline void SetFlag(UINT& v, UINT mask, UINT n)
{
    _ASSERT((~mask & n) == 0);
    v = (~mask & v) | (mask & n);
}

inline LONG Width(const RECT r)
{
    return r.right - r.left;
}

inline LONG Height(const RECT r)
{
    return r.bottom - r.top;
}

inline HWND Edit_Create(HWND hParent, DWORD dwStyle, RECT rc, int id)
{
    HWND hWnd = CreateWindow(
        WC_EDIT,
        TEXT(""),
        dwStyle,
        rc.left, rc.top, Width(rc), Height(rc),
        hParent,
        (HMENU) (INT_PTR) id,
        NULL,
        NULL);
    CHECK_LE(hWnd != NULL);
    return hWnd;
}

#define ID_EDIT 213

class TextDocWindow : public MDIChild
{
    friend WindowManager<TextDocWindow>;
    using Class = ChildClass;
public:
    struct Init
    {
        HFONT hFont;
        LPCTSTR pFileName;
        BOOL Maximized;
    };
    static ATOM Register() { return ::Register<Class>(); }
    static TextDocWindow* Create(HWND hWndParent, const Init& init) { return WindowManager<TextDocWindow>::Create(hWndParent, TEXT("New Document"), (LPVOID) (INT_PTR) &init); }

protected:
    virtual HWND CreateWnd(const CREATESTRUCT& ocs)
    {
        const Init& init = *((Init*) (INT_PTR) ocs.lpCreateParams);
        CREATESTRUCT cs = ocs;
        if (init.Maximized)
            cs.style |= WS_MAXIMIZE;
        return MDIChild::CreateWnd(cs);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_COMMAND, OnCommand);
            HANDLE_MSG(WM_SIZE, OnSize);
            HANDLE_MSG(WM_SETFOCUS, OnSetFocus);
            HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
        }

        if (!IsHandled())
            ret = MDIChild::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);
    void OnSize(UINT state, int cx, int cy);
    void OnSetFocus(HWND hwndOldFocus);
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);

private:
    void SetTitle()
    {
        stdt::string title = m_FileName.empty() ? TEXT("Untitled") : PathFindFileName(m_FileName.c_str());
        if (m_modified)
            title += TEXT('*');
        SetWindowText(*this, title.c_str());
    }

    bool SelectFileName()
    {
        OPENFILENAME ofn = {};
        TCHAR szFileName[MAX_PATH] = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = *this;
        ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = ARRAYSIZE(szFileName);
        ofn.lpstrTitle = TEXT("Save Text File");
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        if (GetSaveFileName(&ofn))
        {
            m_FileName = szFileName;
            return true;
        }
        else
            return false;
    }

    void Save()
    {
        _ASSERT(!m_FileName.empty());
        // TODO Use text control handle
        stdt::string text;
        text.resize(GetWindowTextLength(m_hWndChild));
        GetWindowText(m_hWndChild, text.data(), static_cast<int>(text.size() + 1));

        switch (m_LineEndings)
        {
        case LineEndings::Unix:         replaceAll(text, TEXT("\r\n"), TEXT("\n")); break;
        case LineEndings::Macintosh:    replaceAll(text, TEXT("\r\n"), TEXT("\r")); break;
        }

#ifdef UNICODE
        const UINT cp = CP_UTF16_LE;
#else
        const UINT cp = CP_ACP;
#endif
        RadOTextFile otf(m_FileName.c_str(), CP_ACP, true);
        if (!CHECK_LE(otf.Valid())) return; // TODO This should be a user friendly message
        otf.Write(text, cp);

        m_modified = false;
        SetTitle();
    }

    void SetModified()
    {
        if (!m_modified)
        {
            m_modified = true;
            SetTitle();
        }
    }

    HFONT m_hFont = NULL;
    stdt::string m_FileName;
    HWND m_hWndChild = NULL;
    bool m_modified = false;

    enum class LineEndings { Windows, Unix, Macintosh };
    LineEndings m_LineEndings = LineEndings::Windows;
};

BOOL TextDocWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    const Init& init = *((Init*) (INT_PTR) lpCreateStruct->lpCreateParams);
    m_hFont = init.hFont;
    m_FileName = init.pFileName ? init.pFileName : TEXT("");
    m_hWndChild = Edit_Create(*this, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE, RECT(), ID_EDIT);
    SetWindowFont(m_hWndChild, m_hFont, TRUE);

    if (!m_FileName.empty())
    {
#ifdef UNICODE
        const UINT cp = CP_UTF16_LE;
#else
        const UINT cp = CP_ACP;
#endif
        stdt::string fullfile;
        {
            int windowsle = 0;
            int linuxle = 0;
            int macosle = 0;

            stdt::string line;
            RadITextFile itf(m_FileName.c_str(), CP_ACP);
            CHECK_LE_RET(itf.Valid(), FALSE); // TODO This should be a user friendly message
            while (itf.ReadLine(line, cp))
            {
                if (right(line, 2) == TEXT("\r\n"))
                    ++windowsle;
                else if (right(line, 1) == TEXT("\n"))
                {
                    ++linuxle;
                    line.insert(line.size() - 1, 1, TEXT('\r'));
                }
                else if (right(line, 1) == TEXT("\r"))
                {
                    ++macosle;
                    line.insert(line.size(), 1, TEXT('\n'));
                }
                fullfile += line;
            }

            if ((windowsle != 0 && linuxle != 0) || (windowsle != 0 && macosle != 0) || (linuxle != 0 && macosle != 0))
            {
                MessageBox(*this, TEXT("File has mixed line endings. Converting to windows."), g_ProjectTitle, MB_ICONASTERISK | MB_OK);
                m_LineEndings = LineEndings::Windows;
            }
            else if (windowsle > 0)
                m_LineEndings = LineEndings::Windows;
            else if (linuxle > 0)
                m_LineEndings = LineEndings::Unix;
            else if (macosle > 0)
                m_LineEndings = LineEndings::Macintosh;
        }
        if (!fullfile.empty())
            Edit_SetText(m_hWndChild, fullfile.c_str());
    }
    m_modified = false;
    SetTitle();

    return TRUE;
}

void TextDocWindow::OnDestroy()
{
}

void TextDocWindow::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_FILE_CLOSE:
        SendMessage(*this, WM_CLOSE, 0, 0);
        // TODO Should we use WM_MDIDESTROY instead?
        break;
    case ID_FILE_SAVE:
        if (!m_FileName.empty() || SelectFileName())
            Save();
        break;
    case ID_FILE_SAVEAS:
        if (SelectFileName())
            Save();
        break;
    case ID_LINEENDINGS_WINDOWS:
        m_LineEndings = LineEndings::Windows;
        SetModified();
        break;
    case ID_LINEENDINGS_UNIX:
        m_LineEndings = LineEndings::Unix;
        SetModified();
        break;
    case ID_LINEENDINGS_MACINTOSH:
        m_LineEndings = LineEndings::Macintosh;
        SetModified();
        break;
    case ID_EDIT:
        switch (codeNotify)
        {
        case EN_CHANGE:
            SetModified();
            break;
        }
        break;
    default:
        SetHandled(false);
        break;
    }
}

void TextDocWindow::OnSize(const UINT state, const int cx, const int cy)
{
    if (m_hWndChild)
    {
        SetWindowPos(m_hWndChild, NULL, 0, 0,
            cx, cy,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void TextDocWindow::OnSetFocus(const HWND hwndOldFocus)
{
    if (m_hWndChild)
        SetFocus(m_hWndChild);
}

void TextDocWindow::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    if (fSystemMenu)
    {
        SetHandled(false);
        return;
    }

    const int count = GetMenuItemCount(hMenu);

    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE | MIIM_ID;
    for (int i = 0; i < count; ++i)
    {
        CHECK_LE(GetMenuItemInfo(hMenu, i, TRUE, &mii));
        switch (mii.wID)
        {
        case ID_FILE_SAVE:
        case ID_FILE_SAVEAS:
            SetFlag(mii.fState, MFS_ENABLED | MFS_DISABLED, m_modified ? MFS_ENABLED : MFS_DISABLED);
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
            break;

        case ID_LINEENDINGS_WINDOWS:
            SetFlag(mii.fState, MFS_CHECKED | MFS_UNCHECKED, m_LineEndings == LineEndings::Windows ? MFS_CHECKED : MFS_UNCHECKED);
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
            break;
        case ID_LINEENDINGS_UNIX:
            SetFlag(mii.fState, MFS_CHECKED | MFS_UNCHECKED, m_LineEndings == LineEndings::Unix ? MFS_CHECKED : MFS_UNCHECKED);
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
            break;
        case ID_LINEENDINGS_MACINTOSH:
            SetFlag(mii.fState, MFS_CHECKED | MFS_UNCHECKED, m_LineEndings == LineEndings::Macintosh ? MFS_CHECKED : MFS_UNCHECKED);
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
            break;
        }
    }
}

class RootWindow : public MDIFrame
{
    friend WindowManager<RootWindow>;
    using Class = MainMDIFrameClass;
public:
    static ATOM Register() { return ::Register<Class>(); }
    static RootWindow* Create() { return WindowManager<RootWindow>::Create(NULL, g_ProjectTitle); }

protected:
    virtual HWND CreateWnd(const CREATESTRUCT& ocs)
    {
        CREATESTRUCT cs = ocs;
        cs.hMenu = LoadMenu(cs.hInstance, MAKEINTRESOURCE(IDR_MENU1));
        return MDIFrame::CreateWnd(cs);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_SIZE, OnSize);
            HANDLE_MSG(WM_COMMAND, OnCommand);
            HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
            HANDLE_MSG(WM_ACTIVATE, OnActivate);
        }

        if (!IsHandled())
            ret = MDIFrame::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    HWND GetActiveChild(BOOL* b = nullptr) const
    {
        HWND hWndChild = (HWND) SendMessage(GetMDIClient(), WM_MDIGETACTIVE, 0, (INT_PTR) b);
        return hWndChild;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnSize(UINT state, int cx, int cy);
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);
    void OnActivate(UINT state, HWND hWndActDeact, BOOL fMinimized);

private:
    HFONT m_hFont = NULL;
    HACCEL m_hAccelTable = NULL;
    HWND m_hStatusBar = NULL;
};

BOOL RootWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    m_hAccelTable = CHECK_LE(LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1)));

    m_hFont = CHECK_LE(CreateFont(
        16, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        TEXT("Consolas")));

    m_hStatusBar = CHECK_LE(CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, TEXT("Ready"), *this, 0));

    return TRUE;
}

void RootWindow::OnDestroy()
{
    DeleteObject(m_hFont);
    PostQuitMessage(0);
}

void RootWindow::OnSize(UINT state, int cx, int cy)
{
    if (m_hStatusBar)
    {
        FORWARD_WM_SIZE(m_hStatusBar, state, cx, cy, SendMessage);
        RECT rcStatusBar;
        CHECK_LE(GetWindowRect(m_hStatusBar, &rcStatusBar));

        RECT rcMDIClient;
        CHECK_LE(GetClientRect(*this, &rcMDIClient));
        rcMDIClient.bottom -= Height(rcStatusBar);
        CHECK_LE(SetWindowPos(GetMDIClient(), NULL, rcMDIClient.left, rcMDIClient.top, Width(rcMDIClient), Height(rcMDIClient), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW));
    }
    else
        SetHandled(false);
}

void RootWindow::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_FILE_NEW:
    {
        BOOL bMaximized = FALSE;
        if (GetActiveChild(&bMaximized) == NULL)
            bMaximized = TRUE;
        CHECK_LE(TextDocWindow::Create(GetMDIClient(), { m_hFont, nullptr, bMaximized }));
        break;
    }
    case ID_FILE_OPEN:
    {
        OPENFILENAME ofn = {};
        TCHAR szFileName[MAX_PATH] = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = *this;
        ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = ARRAYSIZE(szFileName);
        ofn.lpstrTitle = TEXT("Open Text File");
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        if (GetOpenFileName(&ofn))
        {
            BOOL bMaximized = FALSE;
            if (GetActiveChild(&bMaximized) == NULL)
                bMaximized = TRUE;
            /*TextDocWindow* pWnd =*/ TextDocWindow::Create(GetMDIClient(), { m_hFont, szFileName, bMaximized });
        }
        break;
    }
    case ID_FILE_EXIT:
        SendMessage(*this, WM_CLOSE, 0, 0);
        break;
    case ID_WINDOW_TILEVERTICAL:
        TileWindows(GetMDIClient(), MDITILE_VERTICAL, nullptr, 0, nullptr);
        break;
    case ID_WINDOW_TILEHORIZONTAL:
        TileWindows(GetMDIClient(), MDITILE_HORIZONTAL, nullptr, 0, nullptr);
        break;
    case ID_WINDOW_CASCADE:
        CascadeWindows(GetMDIClient(), 0, nullptr, 0, nullptr);
        break;
    default:
    {
        HWND hWndActive = GetActiveChild();
        if (hWndActive)
            FORWARD_WM_COMMAND(hWndActive, id, hWndCtl, codeNotify, SendMessage);
        SetHandled(false);
        break;
    }
    }
}

std::vector<ACCEL> GetAccelerators(HACCEL hAccelTable)
{
    std::vector<ACCEL> accels;
    if (hAccelTable)
    {
        int count = CopyAcceleratorTable(hAccelTable, NULL, 0);
        if (count > 0)
        {
            accels.resize(count);
            CHECK_LE(CopyAcceleratorTable(hAccelTable, accels.data(), count));
        }
    }
    return accels;
}

LPCTSTR ToString(TCHAR c)
{
    thread_local TCHAR str[] = TEXT("_");
    str[0] = c;
    return str;
}

void RootWindow::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    UINT id0 = GetMenuItemID(hMenu, 0);
    if (fSystemMenu || GetMenuItemID(hMenu, 0) == SC_RESTORE)
    {
        SetHandled(false);
        return;
    }

    const HWND hWndActive = GetActiveChild();

    std::vector<ACCEL> accels;

    TCHAR label[100] = {};
    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE | MIIM_ID | MIIM_STRING;
    mii.dwTypeData = label;
    const int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i)
    {
        bool changed = false;
        mii.cch = ARRAYSIZE(label);
        CHECK_LE(GetMenuItemInfo(hMenu, i, TRUE, &mii));

        switch (mii.wID)
        {
        case 0xFFFF:    // SubMenu
            break;

        case ID_FILE_NEW:
        case ID_FILE_OPEN:
        case ID_FILE_EXIT:
            break;

        default:
            SetFlag(mii.fState, MFS_ENABLED | MFS_DISABLED, hWndActive ? MFS_ENABLED : MFS_DISABLED);
            changed = true;
            break;
        }

        if (m_hAccelTable && _tcschr(label, TEXT('\t')) == nullptr)
        {
            if (accels.empty())
                accels = GetAccelerators(m_hAccelTable);
            CHECK_HR(StringCchCat(label, ARRAYSIZE(label), TEXT("\t")));
            auto it = std::find_if(accels.begin(), accels.end(), [&mii](const ACCEL& a) { return a.cmd == mii.wID; });
            if (it != accels.end())
            {
                if (it->fVirt & FCONTROL)
                    CHECK_HR(StringCchCat(label, ARRAYSIZE(label), TEXT("Ctrl+")));
                if (it->fVirt & FALT)
                    CHECK_HR(StringCchCat(label, ARRAYSIZE(label), TEXT("Alt+")));
                if (it->fVirt & FSHIFT)
                    CHECK_HR(StringCchCat(label, ARRAYSIZE(label), TEXT("Shift+")));
                // TODO Wont wont work for non-ASCII characters
                CHECK_HR(StringCchCat(label, ARRAYSIZE(label), ToString(it->key)));
                changed = true;
            }
        }

        if (changed)
            CHECK_LE(SetMenuItemInfo(hMenu, i, TRUE, &mii));
    }

    if (hWndActive)
        FORWARD_WM_INITMENUPOPUP(hWndActive, hMenu, item, fSystemMenu, SendMessage);
}

void RootWindow::OnActivate(UINT state, HWND hWndActDeact, BOOL fMinimized)
{
    if (state)
    {
        g_hAccelTable = m_hAccelTable;
        g_hWndAccel = *this;
    }
    else if (g_hAccelTable == m_hAccelTable)
    {
        g_hAccelTable = NULL;
        g_hWndAccel = NULL;
    }
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    RadLogInitWnd(NULL, "MultiPad", L"MultiPad");

    CHECK_LE_RET(RootWindow::Register(), false);
    CHECK_LE_RET(TextDocWindow::Register(), false);

    RootWindow* prw = CHECK_LE(RootWindow::Create());
    if (!prw) return false;

    RadLogInitWnd(*prw, nullptr, nullptr);
    ShowWindow(*prw, nShowCmd);

    return true;
}
