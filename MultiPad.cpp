#include "Rad/MDI.h"

#include <CommDlg.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <tchar.h>
#include <strsafe.h>

#include "Rad/Dialog.h"
#include "Rad/Windowxx.h"
#include "Rad/WinError.h"
#include "Rad/MemoryPlus.h"
#include "Rad/RadTextFile.h"
#include "Rad/Format.h"

#include <vector>
#include "resource.h"

#include "CommandStateChain.h"
#include "ShowMenuShortcutChain.h"
#include "EditPlus.h"

// TODO
// Application icon
// Document icon
// Toolbar
// Monitor for file updates
// Multi undo
// Word boundaries
// Save position in registry
// Choose font
// Word wrap
// Open from url
// Recent file list
// goto line
// tab mode
// split view
// tab controls

// TODO - Not sure this can be done with the edit control
// line numbers
// bookmarks

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

inline void StatusBar_SetText(HWND hWndStatusBar, int part, stdt::string_view text)
{
    SendMessage(hWndStatusBar, SB_SETTEXT, MAKEWORD(part, 0), (LPARAM) text.data());
}

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

#define ID_EDIT 213

class TextDocWindow : public MDIChild, protected CommandState
{
    friend WindowManager<TextDocWindow>;
    using Class = ChildClass;
public:
    struct Init
    {
        HFONT hFont;
        HWND hStatusBar;
        LPCTSTR pFileName;
        DWORD cp;
        BOOL Maximized;
    };
    static ATOM Register() { return ::Register<Class>(); }
    static TextDocWindow* Create(HWND hWndParent, const Init& init) { return WindowManager<TextDocWindow>::Create(hWndParent, TEXT("New Document"), (LPVOID) (INT_PTR) &init); }

    bool IsModified() const
    {
        return Edit_GetModify(m_hWndChild);
    }
    const stdt::string& GetFileName() const
    {
        return m_FileName;
    }
    stdt::string GetTitle() const
    {
        if (m_FileName.empty())
            return TEXT("Untitled");
        else
        {
            LPCTSTR pName = PathFindFileName(m_FileName.c_str());
            return stdt::string(pName).append(TEXT(" @ ")).append(stdt::string_view(m_FileName.c_str(), pName - m_FileName.c_str() - 1));
        }
    }

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
            HANDLE_MSG(WM_CLOSE, OnClose);
            HANDLE_MSG(WM_COMMAND, OnCommand);
            HANDLE_MSG(WM_SIZE, OnSize);
            HANDLE_MSG(WM_SETFOCUS, OnSetFocus);
            HANDLE_MSG(WM_MDIACTIVATE, OnMDIActivate);
        }

        MessageChain* Chains[] = { &m_CommandStateChain };
        for (MessageChain* pChain : Chains)
        {
            //if (IsHandled())
                //break;

            bool bHandled = false;
            ret = pChain->ProcessMessage(*this, uMsg, wParam, lParam, bHandled);
            if (bHandled)
                SetHandled(true);
        }
        if (!IsHandled())
            ret = MDIChild::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnClose();
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);
    void OnSize(UINT state, int cx, int cy);
    void OnSetFocus(HWND hwndOldFocus);
    void OnMDIActivate(HWND hWndActivate, HWND hWndDeactivate);

private:
    void SetTitle()
    {
        stdt::string title = GetTitle();
        if (Edit_GetModify(m_hWndChild))
            title += TEXT('*');
        TCHAR currentTitle[MAX_PATH] = {};
        GetWindowText(*this, currentTitle, ARRAYSIZE(currentTitle));
        if (title != currentTitle)
            SetWindowText(*this, title.c_str());
    }

public:
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

    bool Save()
    {
        _ASSERT(!m_FileName.empty());
        // TODO Use text control handle
        stdt::string text;
        text.resize(GetWindowTextLength(m_hWndChild));
        GetWindowText(m_hWndChild, text.data(), static_cast<int>(text.size() + 1));

        // https://en.wikipedia.org/wiki/Unicode_control_characters
        for (TCHAR& c : text)
        {
            if (c == 0x2421)
                c = 0x7F;
            if (c >= 0x2410 && c < 0x2420)
                c -= 0x2410;
        }

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
        RadOTextFile otf(m_FileName.c_str(), m_cp, true);
        CHECK_LE_RET(otf.Valid(), false); // TODO This should be a user friendly message
        otf.Write(text, cp);

        Edit_SetModify(m_hWndChild, false);
        SetTitle();
        return true;
    }

private:
    void SetModified()
    {
        if (!Edit_GetModify(m_hWndChild))
        {
            Edit_SetModify(m_hWndChild, true);
            SetTitle();
        }
    }

    void GetState(UINT id, State& state) const;

    void SetStatusBarText()
    {
        DWORD nCaretPos, nSelStart, nSelEnd;
        EditEx_GetCaret(m_hWndChild, &nCaretPos);
        Edit_GetSelEx(m_hWndChild, &nSelStart, &nSelEnd);
        _ASSERT(nCaretPos == nSelStart || nCaretPos == nSelEnd);
        const POINT editpos = EditGetPos(m_hWndChild, nCaretPos);
        StatusBar_SetText(m_hStatusBar, 1, nSelEnd == nSelStart
            ? Format(TEXT("Ln %d, Col %d"), editpos.y, editpos.x)
            : Format(TEXT("Ln %d, Col %d, Sel %d"), editpos.y, editpos.x, nSelEnd - nSelStart));
    }

    HFONT m_hFont = NULL;
    HWND m_hStatusBar = NULL;
    stdt::string m_FileName;
    HWND m_hWndChild = NULL;

    enum class LineEndings { Windows, Unix, Macintosh };
    UINT m_cp = CP_ACP;
    LineEndings m_LineEndings = LineEndings::Windows;

    CommandStateChain m_CommandStateChain;
};

BOOL TextDocWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    const Init& init = *((Init*) (INT_PTR) lpCreateStruct->lpCreateParams);
    m_hFont = init.hFont;
    m_hStatusBar = init.hStatusBar;
    m_FileName = init.pFileName ? init.pFileName : TEXT("");
    m_cp = init.cp;

    m_CommandStateChain.Init(this);

    m_hWndChild = Edit_Create(*this, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE, RECT(), ID_EDIT);
    InitEditEx(m_hWndChild);
    SetWindowFont(m_hWndChild, m_hFont, TRUE);
    Edit_LimitText(m_hWndChild, 0);

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
            RadITextFile itf(m_FileName.c_str(), m_cp);
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
                for (TCHAR& c : line)
                {
                    switch (c)
                    {
                    case TEXT('\r'):
                    case TEXT('\n'):
                        break;

                    case TEXT(' '):
                        //c = 0x2423;
                        break;
                    case TEXT('\t'):
                        break;

                    case 0x7F:  // DEL
                        c = 0x2421;
                        break;

                    default:
                        if (_istcntrl(c))
                            c += 0x2410;
                        break;
                    }
                }
                fullfile += line;
            }

            m_cp = itf.GetCodePage();
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
    Edit_SetModify(m_hWndChild, false);
    SetTitle();

    return TRUE;
}

void TextDocWindow::OnClose()
{
    if (Edit_GetModify(m_hWndChild))
    {
        const int ret = MessageBox(*this, TEXT("Do you want to save changes?"), g_ProjectTitle, MB_ICONQUESTION | MB_YESNOCANCEL);
        if (ret == IDCANCEL)
        {
            return;
        }
        else if (ret == IDYES)
        {
            if (!Save())
                return;
        }
    }
    SetHandled(false);
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
    case ID_ENCODING_ANSI:
        m_cp = CP_ACP;
        SetModified();
        break;
    case ID_ENCODING_UTF8:
        m_cp = CP_UTF8;
        SetModified();
        break;
    case ID_ENCODING_UTF16_BE:
        m_cp = CP_UTF16_BE;
        SetModified();
        break;
    case ID_ENCODING_UTF16_LE:
        m_cp = CP_UTF16_LE;
        SetModified();
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
    case ID_VIEW_WHITESPACE:
    {
        const BOOL bViewWhitespace = EditEx_GetViewWhiteSpace(m_hWndChild);
        EditEx_SetViewWhiteSpace(m_hWndChild, !bViewWhitespace);
        break;
    }
    case ID_EDIT:
        switch (codeNotify)
        {
        case EN_CHANGE:
            //SetModified();
            SetTitle();
            break;
        case EN_SEL_CHANGED:
            SetStatusBarText();
            break;
        case EN_ERRSPACE:
            MessageBox(*this, TEXT("Not enough memory to complete the operation."), g_ProjectTitle, MB_ICONERROR | MB_OK);
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

void TextDocWindow::OnMDIActivate(HWND hWndActivate, HWND hWndDeactivate)
{
    if (hWndActivate == *this)
        SetStatusBarText();
    else if (hWndActivate == NULL)
        StatusBar_SetText(m_hStatusBar, 1, TEXT(""));
}

void TextDocWindow::GetState(UINT id, State& state) const
{
    switch (id)
    {
    case ID_FILE_SAVE:              state.enabled = Edit_GetModify(m_hWndChild); break;
    case ID_ENCODING_ANSI:          state.checked = m_cp == CP_ACP; break;
    case ID_ENCODING_UTF8:          state.checked = m_cp == CP_UTF8; break;
    case ID_ENCODING_UTF16_BE:      state.checked = m_cp == CP_UTF16_BE; break;
    case ID_ENCODING_UTF16_LE:      state.checked = m_cp == CP_UTF16_LE; break;
    case ID_LINEENDINGS_WINDOWS:    state.checked = m_LineEndings == LineEndings::Windows; break;
    case ID_LINEENDINGS_UNIX:       state.checked = m_LineEndings == LineEndings::Unix; break;
    case ID_LINEENDINGS_MACINTOSH:  state.checked = m_LineEndings == LineEndings::Macintosh;break;
    case ID_VIEW_WHITESPACE:        state.checked = EditEx_GetViewWhiteSpace(m_hWndChild); break;
    }
}

class RootWindow : public MDIFrame, protected CommandState
{
    friend WindowManager<RootWindow>;
    using Class = MainMDIFrameClass;
public:
    static ATOM Register() { return ::Register<Class>(); }
    static RootWindow* Create() { return WindowManager<RootWindow>::Create(NULL, g_ProjectTitle); }

    void OpenFile(LPCTSTR filename, DWORD cp = CP_ACP)
    {
        BOOL bMaximized = FALSE;
        if (GetActiveChild(&bMaximized) == NULL)
            bMaximized = TRUE;
        /*TextDocWindow* pWnd =*/ CHECK_LE(TextDocWindow::Create(GetMDIClient(), { m_hFont, m_hStatusBar, filename, cp, bMaximized }));
    }

protected:
    virtual HWND CreateWnd(const CREATESTRUCT& ocs)
    {
        CREATESTRUCT cs = ocs;
        cs.hMenu = LoadMenu(cs.hInstance, MAKEINTRESOURCE(IDR_MENU1));
        cs.dwExStyle |= WS_EX_ACCEPTFILES;
        return MDIFrame::CreateWnd(cs);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_CLOSE, OnClose);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_SIZE, OnSize);
            HANDLE_MSG(WM_COMMAND, OnCommand);
            HANDLE_MSG(WM_ACTIVATE, OnActivate);
            HANDLE_MSG(WM_DROPFILES, OnDropFiles);
        }

        MessageChain* Chains[] = { &m_CommandStateChain, &m_ShowMenuShortcutChain };
        for (MessageChain* pChain : Chains)
        {
            //if (IsHandled())
                //break;

            bool bHandled = false;
            ret = pChain->ProcessMessage(*this, uMsg, wParam, lParam, bHandled);
            if (bHandled)
                SetHandled(true);
        }
        if (uMsg == WM_INITMENUPOPUP)
        {
            const HWND hWndActive = GetActiveChild();
            if (hWndActive)
                SendMessage(hWndActive, uMsg, wParam, lParam);
        }
        if (!IsHandled())
            ret = MDIFrame::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnClose();
    void OnDestroy();
    void OnSize(UINT state, int cx, int cy);
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);
    void OnActivate(UINT state, HWND hWndActDeact, BOOL fMinimized);
    void OnDropFiles(HDROP hdrop);

private:
    HWND GetActiveChild(BOOL* b = nullptr) const
    {
        HWND hWndChild = (HWND) SendMessage(GetMDIClient(), WM_MDIGETACTIVE, 0, (INT_PTR) b);
        return hWndChild;
    }

    bool SaveAll(const std::vector<TextDocWindow*>& children)
    {
        bool bSuccess = true;
        for (TextDocWindow* pDoc : children)
        {
            if (!pDoc->GetFileName().empty() || pDoc->SelectFileName())
            {
                if (!pDoc->Save())
                    bSuccess = false;
            }
            else
                bSuccess = false;
        }
        if (!bSuccess)
            MessageBox(*this, TEXT("Failed to save some documents."), g_ProjectTitle, MB_ICONERROR | MB_OK);
        return bSuccess;
    }

    std::vector<TextDocWindow*> GetModifiedChildren() const
    {
        std::vector<TextDocWindow*> children;
        EnumChildWindows(GetMDIClient(), [](HWND hWnd, LPARAM lParam) -> BOOL
            {
                std::vector<TextDocWindow*>& children = *(std::vector<TextDocWindow*>*) lParam;
                MessageHandler* pmh = MessageHandler::GetFrom(hWnd);
                TextDocWindow* pDoc = dynamic_cast<TextDocWindow*>(pmh);
                if (pDoc && pDoc->IsModified())
                    children.push_back(pDoc);
                return TRUE; // Continue enumeration
            }, (LPARAM) &children);
        return children;
    }

    void GetState(UINT id, State& state) const;

    HFONT m_hFont = NULL;
    HACCEL m_hAccelTable = NULL;
    HWND m_hStatusBar = NULL;

    CommandStateChain m_CommandStateChain;
    ShowMenuShortcutChain m_ShowMenuShortcutChain;
};

BOOL RootWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    m_CommandStateChain.Init(this);

    m_hAccelTable = CHECK_LE(LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1)));

    m_hFont = CHECK_LE(CreateFont(
        20, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        TEXT("Consolas")));

    m_hStatusBar = CHECK_LE(CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, TEXT("Ready"), *this, 0));

    m_ShowMenuShortcutChain.Init(m_hAccelTable);

    return TRUE;
}

void RootWindow::OnClose()
{
    const std::vector<TextDocWindow*> children = GetModifiedChildren();
    if (!children.empty())
    {
        stdt::string message = Format(TEXT("The following documents have unsaved changes:\n"));
        for (TextDocWindow* pDoc : children)
            message += Format(TEXT("  %s\n"), pDoc->GetTitle().c_str());
        message += TEXT("Do you want to close them anyway?");
        const int ret = MessageBox(*this, message.c_str(), g_ProjectTitle, MB_ICONQUESTION | MB_YESNOCANCEL);
        if (ret == IDCANCEL)
            return;
        else if (ret == IDYES)
        {
            if (!SaveAll(children))
                return;
        }
    }
    SetHandled(false);
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
        const int partsizes[] = { 200 };
        {
            std::vector<int> parts(ARRAYSIZE(partsizes) + 1);
            parts[ARRAYSIZE(partsizes)] = -1; // Right edge
            int r = cx;
            for (int i = ARRAYSIZE(partsizes) - 1; i >= 0; --i)
            {
                r -= partsizes[i];
                parts[i] = r;
            }
            SendMessage(m_hStatusBar, SB_SETPARTS, parts.size(), (LPARAM) parts.data());
        }

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

bool FileOpenDialog(HWND hWndOwner, std::wstring& filename, DWORD& cp);

void RootWindow::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_FILE_NEW:
        OpenFile(nullptr);
        break;
    case ID_FILE_OPEN:
    {
#if 1
        stdt::string filename;
        DWORD cp = CP_ACP;
        if (FileOpenDialog(*this, filename, cp))
            OpenFile(filename.c_str(), cp);
#else
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
            OpenFile(szFileName);
#endif
        break;
    }
    case ID_FILE_SAVEALL:
    {
        const std::vector<TextDocWindow*> children = GetModifiedChildren();
        if (!children.empty())
            SaveAll(children);
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

void RootWindow::GetState(UINT id, State& state) const
{
    switch (id)
    {
    case ID_FILE_NEW:
    case ID_FILE_OPEN:
    case ID_FILE_EXIT:
        break;

    case ID_FILE_SAVEALL:
        state.enabled = !GetModifiedChildren().empty();
        break;

    default:
        state.enabled = GetActiveChild();
        break;
    }
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

void RootWindow::OnDropFiles(HDROP hdrop)
{
    const UINT count = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < count; ++i)
    {
        TCHAR filename[MAX_PATH] = {};
        if (DragQueryFile(hdrop, i, filename, ARRAYSIZE(filename)))
            OpenFile(filename);
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

    for (int arg = 1; arg < __argc; ++arg)
    {
        const stdt::string_view filename = __targv[arg];
        if (!filename.empty())
            prw->OpenFile(filename.data());
    }

    return true;
}
