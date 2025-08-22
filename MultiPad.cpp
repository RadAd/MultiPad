#include "Rad/MDI.h"

#include <CommDlg.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <cctype>

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
#include "GdiPlus.h"
#include "RegUtils.h"

// TODO
// Application icon
// Document icon
// Toolbar
// Monitor for file updates
// Multi undo
// Save position in registry
// Choose font
// Open from url
// Recent file list
// goto line
// tab mode
// split view
// readonly mode
// ES_EX_CONVERT_EOL_ON_PASTE
// Use right gutter to show mismatched EOL and whitespace at end of line

#define __istcsym(c)  (_istalnum(c) || ((c) == TEXT('_')))

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

#define STATUS_EOL 1
#define STATUS_POS 2

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

inline int WordBreakProc(_In_ LPCTSTR lpch, _In_ int ichCurrent, _In_ const int cch, _In_ const int code)
{
    switch (code)
    {
    case WB_ISDELIMITER:
        return !__istcsym(lpch[ichCurrent]) ? TRUE : FALSE;
    case WB_LEFT:
        if (ichCurrent > 0)
            --ichCurrent;
        if (_istblank(lpch[ichCurrent]))    // TODO Need to take into account substitutued whitespace
        {
            while (ichCurrent > 0 && _istblank(lpch[ichCurrent - 1]))
                --ichCurrent;
        }
        else if (__istcsym(lpch[ichCurrent]))
        {
            while (ichCurrent > 0 && __istcsym(lpch[ichCurrent - 1]))
                --ichCurrent;
        }
        return ichCurrent;
    case WB_RIGHT:
        if (_istblank(lpch[ichCurrent]))    // TODO Need to take into account substitutued whitespace
        {
            while (ichCurrent < cch && _istblank(lpch[ichCurrent]))
                ++ichCurrent;
        }
        else if (__istcsym(lpch[ichCurrent]))
        {
            while (ichCurrent < cch && __istcsym(lpch[ichCurrent]))
                ++ichCurrent;
        }
        return ichCurrent;
    default:
        _ASSERT(false);
        return 0;
    }
}

#define ID_EDIT 213
#define ID_TABCTRL 312

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
    stdt::string GetShortTitle() const
    {
        if (m_FileName.empty())
            return TEXT("Untitled");
        else
            return PathFindFileName(m_FileName.c_str());
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
            HANDLE_MSG(WM_DESTROY, OnDestroy);
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

            ChainMessage(pChain);
        }
        if (!IsHandled())
            ret = MDIChild::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnClose();
    void OnDestroy();
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

    void SetStatusBarEolText()
    {
        const LPCTSTR sEol[4] = { TEXT("?"), TEXT("CRLF"), TEXT("LF"), TEXT("CR") };
        const EC_ENDOFLINE eol = Edit_GetEndOfLine(m_hWndChild);
        StatusBar_SetText(m_hStatusBar, STATUS_EOL, sEol[eol]);
    }

    void SetStatusBarPosText()
    {
        const DWORD nCaretPos = EditEx_GetCaret(m_hWndChild);
        DWORD nSelStart, nSelEnd;
        Edit_GetSelEx(m_hWndChild, &nSelStart, &nSelEnd);
        _ASSERT(nCaretPos == nSelStart || nCaretPos == nSelEnd);
        const POINT editpos = EditGetPos(m_hWndChild, nCaretPos);
        StatusBar_SetText(m_hStatusBar, STATUS_POS, nSelEnd == nSelStart
            ? Format(TEXT("Ln %d, Col %d"), editpos.y, editpos.x)
            : Format(TEXT("Ln %d, Col %d, Sel %d"), editpos.y, editpos.x, nSelEnd - nSelStart));
    }

    HFONT m_hFont = NULL;
    HWND m_hStatusBar = NULL;
    stdt::string m_FileName;
    HWND m_hWndChild = NULL;

    UINT m_cp = CP_ACP;

    int m_dwTabWidth = 4;

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
    Edit_SetExtendedStyle(m_hWndChild, ES_EX_ALLOWEOL_ALL, ES_EX_ALLOWEOL_ALL);
    SetWindowFont(m_hWndChild, m_hFont, TRUE);
    int dwTabWidth = m_dwTabWidth * 4;
    Edit_SetTabStops(m_hWndChild, 1, &dwTabWidth);
    Edit_SetWordBreakProc(m_hWndChild, WordBreakProc);
    Edit_LimitText(m_hWndChild, 0);

    if (!m_FileName.empty())
    {
#ifdef UNICODE
        const UINT cp = CP_UTF16_LE;
#else
        const UINT cp = CP_ACP;
#endif
        int LineEnding[4] = { 0, 0, 0, 0, };
        stdt::string fullfile;
        {

            stdt::string line;
            RadITextFile itf(m_FileName.c_str(), m_cp);
            CHECK_LE_RET(itf.Valid(), FALSE); // TODO This should be a user friendly message
            while (itf.ReadLine(line, cp))
            {
                if (right(line, 2) == TEXT("\r\n"))
                    ++LineEnding[EC_ENDOFLINE_CRLF];
                else if (right(line, 1) == TEXT("\n"))
                {
                    ++LineEnding[EC_ENDOFLINE_CR];
                    line.insert(line.size() - 1, 1, TEXT('\r'));
                }
                else if (right(line, 1) == TEXT("\r"))
                {
                    ++LineEnding[EC_ENDOFLINE_LF];
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
        }
        if (!fullfile.empty())
        {
            Edit_SetEndOfLine(m_hWndChild, EC_ENDOFLINE_DETECTFROMCONTENT);
            Edit_SetText(m_hWndChild, fullfile.c_str());
        }
        else
            Edit_SetEndOfLine(m_hWndChild, EC_ENDOFLINE_CRLF);

        if ((LineEnding[EC_ENDOFLINE_CRLF] != 0 && LineEnding[EC_ENDOFLINE_CR] != 0) || (LineEnding[EC_ENDOFLINE_CRLF] != 0 && LineEnding[EC_ENDOFLINE_LF] != 0) || (LineEnding[EC_ENDOFLINE_CR] != 0 && LineEnding[EC_ENDOFLINE_LF] != 0))
        {
            if (MessageBox(*this, TEXT("File has mixed line endings. Do you wish to convert to Windows (CRLF)?"), g_ProjectTitle, MB_ICONASTERISK | MB_YESNO) == IDYES)
            {
                Edit_SetEndOfLine(m_hWndChild, EC_ENDOFLINE_CRLF);
                Edit_ReplaceLineEndings(m_hWndChild);
            }
        }
        SetStatusBarEolText();
    }
    Edit_SetModify(m_hWndChild, false);
    SetTitle();

    SendMessage(GetParent(GetParent(*this)), WM_PARENTNOTIFY, WM_MDICREATE, (LPARAM) HWND(*this));

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

void TextDocWindow::OnDestroy()
{
    SendMessage(GetParent(GetParent(*this)), WM_PARENTNOTIFY, WM_MDIDESTROY, (LPARAM) HWND(*this));
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
        Edit_SetEndOfLine(m_hWndChild, EC_ENDOFLINE_CRLF);
        Edit_ReplaceLineEndings(m_hWndChild);
        SetStatusBarEolText();
        break;
    case ID_LINEENDINGS_UNIX:
        Edit_SetEndOfLine(m_hWndChild, EC_ENDOFLINE_CR);
        Edit_ReplaceLineEndings(m_hWndChild);
        SetStatusBarEolText();
        break;
    case ID_LINEENDINGS_MACINTOSH:
        Edit_SetEndOfLine(m_hWndChild, EC_ENDOFLINE_LF);
        Edit_ReplaceLineEndings(m_hWndChild);
        SetStatusBarEolText();
        break;
    case ID_EDIT_UNDO:
        if (Edit_CanUndo(m_hWndChild))
            Edit_Undo(m_hWndChild);
        break;
    case ID_EDIT_REDO:
#if 0   // TODO
        if (Edit_CanRedo(m_hWndChild))
            Edit_Redo(m_hWndChild);
#endif
        break;
    case ID_EDIT_CUT:
        SendMessage(m_hWndChild, WM_CUT, 0, 0);
        break;
    case ID_EDIT_COPY:
        SendMessage(m_hWndChild, WM_COPY, 0, 0);
        break;
    case ID_EDIT_PASTE:
        SendMessage(m_hWndChild, WM_PASTE, 0, 0);
        break;
    case ID_EDIT_SELECTALL:
        Edit_SetSel(m_hWndChild, 0, Edit_GetTextLength(m_hWndChild));
        break;
    case ID_VIEW_WORDWRAP:
    {
        DWORD dwStyle = GetWindowStyle(m_hWndChild);
        dwStyle ^= WS_HSCROLL;
        HWND hNewEdit = Edit_Create(*this, dwStyle, RECT(), ID_EDIT);
        if (hNewEdit)
        {
            InitEditEx(hNewEdit);
            Edit_SetExtendedStyle(m_hWndChild, Edit_GetExtendedStyle(m_hWndChild), 0xFFFF);
            EditEx_SetStyle(hNewEdit, EditEx_GetStyle(m_hWndChild));
            SetWindowFont(hNewEdit, GetWindowFont(m_hWndChild), TRUE);
            // TODO Get tab width
            int dwTabWidth = m_dwTabWidth * 4;
            Edit_SetTabStops(hNewEdit, 1, &dwTabWidth);
            Edit_SetWordBreakProc(hNewEdit, Edit_GetWordBreakProc(m_hWndChild));
            Edit_LimitText(hNewEdit, Edit_GetLimitText(m_hWndChild));
            //Edit_SetMargins(hNewEdit, EC_LEFTMARGIN | EC_RIGHTMARGIN, Edit_GetMargins(m_hWndChild));
            {
                RECT rc;
                Edit_GetRect(m_hWndChild, &rc);
                Edit_SetRect(hNewEdit, &rc);
            }

            DWORD dwSelStart, dwSelEnd;
            Edit_GetSelEx(m_hWndChild, &dwSelStart, &dwSelEnd);
#if 1
            const HANDLE hNewText = Edit_GetHandle(hNewEdit);
            const HANDLE hText = Edit_GetHandle(m_hWndChild);
            Edit_SetHandle(m_hWndChild, hNewText);
            Edit_SetHandle(hNewEdit, hText);
#else
            int nLength = Edit_GetTextLength(m_hWndChild) + 1;
            LPTSTR pText = new TCHAR[nLength];
            pText[nLength - 1] = TEXT('\0');
            Edit_GetText(m_hWndChild, pText, nLength);
            Edit_SetText(hNewEdit, pText);
            delete[] pText;
#endif
            Edit_SetModify(hNewEdit, Edit_GetModify(m_hWndChild));
            Edit_SetSel(hNewEdit, dwSelStart, dwSelEnd);
            Edit_ScrollCaret(hNewEdit); // TODO Remove when copy scroll pos

#if 0
            const int nFirst = Edit_GetFirstVisibleLine(hNewEdit);
            // Get actual first line number
            // TODO How to reconcile that word wrap mode shows less actual lines than non-word wrap mode?
            Edit_Scroll(hNewEdit, nFirst, 0);
#endif

            // TODO copy caret position, scroll position, margins, password char, readonly, tabstops, word break, undo buffer, etc.

            {
                RECT rc;
                CHECK_LE(GetWindowRect(m_hWndChild, &rc));
                CHECK_LE(ScreenToClient(*this, &rc));
                CHECK_LE(SetWindowPos(hNewEdit, NULL, rc.left, rc.top, Width(rc), Height(rc), SWP_NOZORDER | SWP_NOACTIVATE));
            }

            DestroyWindow(m_hWndChild);
            m_hWndChild = hNewEdit;
            SetFocus(m_hWndChild);
            SetStatusBarPosText();
        }
        break;
    }
    case ID_VIEW_WHITESPACE:
    {
        DWORD dwExStyle = EditEx_GetStyle(m_hWndChild);
        dwExStyle ^= ES_EX_VIEWWHITESPACE;
        EditEx_SetStyle(m_hWndChild, dwExStyle);
        break;
    }
    case ID_VIEW_LINENUMBERS:
    {
        DWORD dwExStyle = EditEx_GetStyle(m_hWndChild);
        dwExStyle ^= ES_EX_LINENUMBERS;
        EditEx_SetStyle(m_hWndChild, dwExStyle);
        break;
    }
    case ID_VIEW_USETABS:
    {
        DWORD dwExStyle = EditEx_GetStyle(m_hWndChild);
        dwExStyle ^= ES_EX_USETABS;
        EditEx_SetStyle(m_hWndChild, dwExStyle);
        break;
    }
    case ID_TABSIZE_1: case ID_TABSIZE_2: case ID_TABSIZE_3: case ID_TABSIZE_4:
    case ID_TABSIZE_5: case ID_TABSIZE_6: case ID_TABSIZE_7: case ID_TABSIZE_8:
    {
        m_dwTabWidth = id - ID_TABSIZE_1 + 1;
        int dwTabWidth = m_dwTabWidth * 4;
        Edit_SetTabStops(m_hWndChild, 1, &dwTabWidth);
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
            SetStatusBarPosText();
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
    {
        SetStatusBarEolText();
        SetStatusBarPosText();
        SendMessage(GetParent(GetParent(*this)), WM_PARENTNOTIFY, WM_MDIACTIVATE, (LPARAM) HWND(*this));
    }
    else if (hWndActivate == NULL)
    {
        const DWORD nParts = (DWORD) SendMessage(m_hStatusBar, SB_GETPARTS, 0, 0);
        for (DWORD i = 1; i < nParts; ++i)
            StatusBar_SetText(m_hStatusBar, i, TEXT(""));
    }
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
    case ID_LINEENDINGS_WINDOWS:    state.checked = Edit_GetEndOfLine(m_hWndChild) == EC_ENDOFLINE_CRLF; break;
    case ID_LINEENDINGS_UNIX:       state.checked = Edit_GetEndOfLine(m_hWndChild) == EC_ENDOFLINE_CR; break;
    case ID_LINEENDINGS_MACINTOSH:  state.checked = Edit_GetEndOfLine(m_hWndChild) == EC_ENDOFLINE_LF; break;
    case ID_EDIT_UNDO:              state.enabled = Edit_CanUndo(m_hWndChild); break;
    case ID_EDIT_REDO:              state.enabled = false; break; // TODO Edit_CanRedo(m_hWndChild); break;
    case ID_EDIT_CUT:
    case ID_EDIT_COPY:
    {
        DWORD nSelStart, nSelEnd;
        Edit_GetSelEx(m_hWndChild, &nSelStart, &nSelEnd);
        state.enabled = nSelStart != nSelEnd;
        break;
    }
#ifdef _UNICODE
    case ID_EDIT_PASTE:             state.enabled = IsClipboardFormatAvailable(CF_UNICODETEXT); break;
#else
    case ID_EDIT_PASTE:             state.enabled = IsClipboardFormatAvailable(CF_TEXT); break;
#endif
    case ID_EDIT_SELECTALL:         state.enabled = Edit_GetTextLength(m_hWndChild); break;
    case ID_VIEW_WORDWRAP:          state.checked = (GetWindowStyle(m_hWndChild) & WS_HSCROLL) == 0; break;
    case ID_VIEW_WHITESPACE:        state.checked = EditEx_GetStyle(m_hWndChild) & ES_EX_VIEWWHITESPACE; break;
    case ID_VIEW_LINENUMBERS:       state.checked = EditEx_GetStyle(m_hWndChild) & ES_EX_LINENUMBERS; break;
    case ID_VIEW_USETABS:           state.checked = EditEx_GetStyle(m_hWndChild) & ES_EX_USETABS; break;
    case ID_TABSIZE_1: case ID_TABSIZE_2: case ID_TABSIZE_3: case ID_TABSIZE_4:
    case ID_TABSIZE_5: case ID_TABSIZE_6: case ID_TABSIZE_7: case ID_TABSIZE_8:
                                    state.checked = (m_dwTabWidth == (id - ID_TABSIZE_1 + 1)); break;
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
        struct FindFilename
        {
            LPCTSTR filename;
            TextDocWindow* pFound;
        };
        FindFilename ff = { filename, NULL };
        if (filename)
        {
            EnumChildWindows(GetMDIClient(), [](HWND hWnd, LPARAM lParam) -> BOOL
                {
                    FindFilename& ff = *(FindFilename*) lParam;
                    MessageHandler* pmh = MessageHandler::GetFrom(hWnd);
                    TextDocWindow* pDoc = dynamic_cast<TextDocWindow*>(pmh);
                    if (pDoc && pDoc->GetFileName() == ff.filename)
                    {
                        ff.pFound = pDoc;
                        return FALSE; // Stop enumeration
                    }
                    else
                        return TRUE; // Continue enumeration
                }, (LPARAM) &ff);
        }

        if (ff.pFound)
        {
            SendMessage(GetMDIClient(), WM_MDIACTIVATE, (WPARAM) HWND(*ff.pFound), 0);
        }
        else
        {
            BOOL bMaximized = FALSE;
            if (GetActiveChild(&bMaximized) == NULL)
                bMaximized = TRUE;
            /*TextDocWindow* pWnd =*/ CHECK_LE(TextDocWindow::Create(GetMDIClient(), { m_hFont, m_hStatusBar, filename, cp, bMaximized }));
        }
    }

protected:
    virtual HWND CreateWnd(const CREATESTRUCT& ocs)
    {
        CREATESTRUCT cs = ocs;
        cs.hMenu = LoadMenu(cs.hInstance, MAKEINTRESOURCE(IDR_MENU1));
        cs.dwExStyle |= WS_EX_ACCEPTFILES;

        HKEY hKey = NULL;
        CHECK_HR(RegOpenKeyEx(HKEY_CURRENT_USER, TEXT(R"-(Software\RadSoft\Multipad)-"), 0, KEY_READ, &hKey));
        if (hKey)
        {
            cs.x = RegGetDWORD(hKey, TEXT("x"), cs.x);
            cs.y = RegGetDWORD(hKey, TEXT("y"), cs.y);
            cs.cx = RegGetDWORD(hKey, TEXT("cx"), cs.cx);
            cs.cy = RegGetDWORD(hKey, TEXT("cy"), cs.cy);
            RegCloseKey(hKey);

            // TODO Validate position
        }

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
            HANDLE_MSG(WM_NOTIFY, OnNotify);
            HANDLE_MSG(WM_ACTIVATE, OnActivate);
            HANDLE_MSG(WM_DROPFILES, OnDropFiles);
            HANDLE_MSG(WM_PARENTNOTIFY, OnParentNotify);
        }

        MessageChain* Chains[] = { &m_CommandStateChain, &m_ShowMenuShortcutChain };
        for (MessageChain* pChain : Chains)
        {
            //if (IsHandled())
                //break;

            ChainMessage(pChain);
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
    LRESULT OnNotify(DWORD dwID, LPNMHDR pNmHdr);
    void OnActivate(UINT state, HWND hWndActDeact, BOOL fMinimized);
    void OnDropFiles(HDROP hdrop);
    void OnParentNotify(UINT msg, HWND hWndChild, int idChild);

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

    int GetTabIndex(TextDocWindow* pDoc)
    {
        const int c = TabCtrl_GetItemCount(m_hTabControl);
        for (int i = 0; i < c; ++i)
        {
            TCITEM tci = {};
            tci.mask = TCIF_PARAM;
            TabCtrl_GetItem(m_hTabControl, i, &tci);
            if ((TextDocWindow*) tci.lParam == pDoc)
                return i;
        }
        return -1; // Not found
    }

    HFONT m_hFont = NULL;
    HACCEL m_hAccelTable = NULL;
    HWND m_hTabControl = NULL;
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

    m_hTabControl = CHECK_LE(CreateWindowEx(0, WC_TABCONTROL, NULL, WS_CHILD | WS_VISIBLE | TCS_FOCUSNEVER | TCS_SINGLELINE, 50, 50, 100, 100, *this, (HMENU) ID_TABCTRL, g_hInstance, NULL));
    CHECK_LE(TabCtrl_SetUnicodeFormat(m_hTabControl, TRUE));
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
    RECT rc;
    CHECK_LE(GetWindowRect(*this, &rc));

    HKEY hKey = NULL;
    CHECK_HR(RegCreateKeyEx(HKEY_CURRENT_USER, TEXT(R"-(Software\RadSoft\Multipad)-"), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr));
    if (hKey)
    {
        RegSetDWORD(hKey, TEXT("x"), rc.left);
        RegSetDWORD(hKey, TEXT("y"), rc.top);
        RegSetDWORD(hKey, TEXT("cx"), Width(rc));
        RegSetDWORD(hKey, TEXT("cy"), Height(rc));
        RegCloseKey(hKey);
    }

    DeleteObject(m_hFont);
    PostQuitMessage(0);
}

void RootWindow::OnSize(UINT state, int cx, int cy)
{
    RECT rcMDIClient;
    CHECK_LE(GetClientRect(*this, &rcMDIClient));

    if (m_hTabControl)
    {
        RECT rcTab = {};
        TabCtrl_GetItemRect(m_hTabControl, 0, &rcTab);

        RECT rcTabControl;
        CHECK_LE(GetWindowRect(m_hTabControl, &rcTabControl));
        CHECK_LE(OffsetRect(&rcTabControl, -rcTabControl.left, -rcTabControl.top));
        rcTabControl.bottom = Height(rcTab);
        rcTabControl.right = Width(rcMDIClient);
        rcMDIClient.top += Height(rcTabControl);

        CHECK_LE(SetWindowPos(m_hTabControl, NULL, rcTabControl.left, rcTabControl.top, Width(rcTabControl), Height(rcTabControl), SWP_NOZORDER | SWP_NOACTIVATE));
    }

    if (m_hStatusBar)
    {
        const int partsizes[] = { 50, 150 };
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

        rcMDIClient.bottom -= Height(rcStatusBar);
    }

    CHECK_LE(SetWindowPos(GetMDIClient(), NULL, rcMDIClient.left, rcMDIClient.top, Width(rcMDIClient), Height(rcMDIClient), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW));
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
        // WM_MDITILE
        TileWindows(GetMDIClient(), MDITILE_VERTICAL, nullptr, 0, nullptr);
        break;
    case ID_WINDOW_TILEHORIZONTAL:
        // WM_MDITILE
        TileWindows(GetMDIClient(), MDITILE_HORIZONTAL, nullptr, 0, nullptr);
        break;
    case ID_WINDOW_CASCADE:
        // WM_MDICASCADE 
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

LRESULT RootWindow::OnNotify(DWORD dwID, LPNMHDR pNmHdr)
{
    switch (dwID)
    {
    case ID_TABCTRL:
        switch (pNmHdr->code)
        {
        case TCN_SELCHANGE:
        {
            const int index = TabCtrl_GetCurSel(m_hTabControl);
            if (index >= 0)
            {
                TCITEM tci = {};
                tci.mask = TCIF_PARAM;
                TabCtrl_GetItem(m_hTabControl, index, &tci);
                TextDocWindow* pDoc = (TextDocWindow*) tci.lParam;
                SendMessage(GetMDIClient(), WM_MDIACTIVATE, (WPARAM) HWND(*pDoc), 0);
            }
            break;
        }
        }
        return 0;

    default:
        SetHandled(false);
        return 0;
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

void RootWindow::OnParentNotify(UINT msg, HWND hWndChild, int idChild)
{
    switch (msg)
    {
    case WM_MDICREATE:
    {
        TextDocWindow* pDoc = dynamic_cast<TextDocWindow*>(MessageHandler::GetFrom(hWndChild));
        if (pDoc)
        {
            stdt::string title = pDoc->GetShortTitle();
            TC_ITEM item = {};
            item.mask |= TCIF_TEXT | TCIF_PARAM;
            item.pszText = title.data();
            item.lParam = (LPARAM) pDoc;
            TabCtrl_InsertItem(m_hTabControl, TabCtrl_GetItemCount(m_hTabControl), &item);
        }
        break;
    }
    case WM_MDIACTIVATE:
    {
        TextDocWindow* pDoc = dynamic_cast<TextDocWindow*>(MessageHandler::GetFrom(hWndChild));
        if (pDoc)
        {
            int index = GetTabIndex(pDoc);
            if (index >= 0 && TabCtrl_GetCurSel(m_hTabControl) != index)
                TabCtrl_SetCurSel(m_hTabControl, index);
        }
        break;
    }
    case WM_MDIDESTROY:
    {
        TextDocWindow* pDoc = dynamic_cast<TextDocWindow*>(MessageHandler::GetFrom(hWndChild));
        if (pDoc)
        {
            int index = GetTabIndex(pDoc);
            TabCtrl_DeleteItem(m_hTabControl, index);
        }
        break;
    }
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
