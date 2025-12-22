#include <iostream>
#include <fstream>
#include <algorithm>

#include "debugsystem.h"
#include "main.h"
#include "uimemo.h"

using namespace std;

//-------------------------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UIMEMO_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TUIMemo::TUIMemo(const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color)
    : TUIObject(uipanel, name, x, y, w, h, color, 0, "", UIL)
{
    FUNCTION_TRACE
    keyable = true;

    m_lines.clear();
    m_lines.push_back("");
    m_lineCount = 1;

    m_drawIterator = m_currentIterator = m_lines.begin();
    m_cursorPos.set(0, 0);
    m_scrollOffset.set(0, 0);
    m_charPos = m_desiredCharPos = 0;

    m_viewW = w - 1;
    m_viewH = h;

    m_originalCrc = CalculateCrc32();
    ApplyScrolling();
}

TUIMemo::~TUIMemo()
{
    FUNCTION_TRACE
    m_lines.clear();
}

void TUIMemo::resize(int x, int y, int w, int h)
{
    FUNCTION_TRACE
    this->x = x; this->y = y; this->w = w; this->h = h;
    m_viewW = w - 1;
    m_viewH = h;
    ApplyScrolling();
}

//-------------------------------------------------------------------------------------------

void TUIMemo::draw()
{
    FUNCTION_TRACE
    if (!visible) return;

    int currentY = y;
    TLineIterator it = m_drawIterator;

    for (; (currentY < y + m_viewH) && (it != m_lines.end()); ++currentY, ++it)
    {
        string fullStr = *it;
        setcolor(window, color1);
        mvwprintw(window, currentY, x, "%*s", m_viewW, ""); // Очистка фона строки

        int byteStart = utf8indexsymbols(fullStr, m_scrollOffset.x);
        int byteEnd   = utf8indexsymbols(fullStr, m_scrollOffset.x + m_viewW);

        if (byteStart != byteEnd)
        {
            string visiblePart = fullStr.substr(byteStart, byteEnd - byteStart);
            mvwprintw(window, currentY, x, "%s", visiblePart.c_str());
        }

        // --- Подсветка синтаксиса ---
        size_t pHash = fullStr.find("#");
        size_t pSlash = fullStr.find("//");
        size_t commentPos = string::npos;

        if (pHash != string::npos && pSlash != string::npos) commentPos = min(pHash, pSlash);
        else if (pHash != string::npos) commentPos = pHash;
        else if (pSlash != string::npos) commentPos = pSlash;

        if (commentPos != string::npos)
        {
            setcolor(window, UIcyan);
            if ((int)commentPos < byteStart)
            {
                // Вся видимая часть строки является комментарием
                string sForLen = fullStr.substr(byteStart, byteEnd - byteStart);
                mvwprintw(window, currentY, x, "%s", sForLen.c_str());
            }
            else if ((int)commentPos < byteEnd)
            {
                // Комментарий начинается внутри видимой области
                string sBefore = fullStr.substr(0, commentPos);
                int textLenBefore = utf8length(sBefore); // Теперь передаем переменную, а не временный объект

                string sComment = fullStr.substr(commentPos, byteEnd - commentPos);
                mvwprintw(window, currentY, x + textLenBefore - m_scrollOffset.x, "%s", sComment.c_str());
            }
        }

        // Ограничиваем область поиска для '=' и '[]' текстом до комментария
        string cleanStr = fullStr.substr(0, commentPos);
        int cleanByteEnd = utf8indexsymbols(cleanStr, m_scrollOffset.x + m_viewW);

        // Присваивания
        size_t eqPos = cleanStr.find("=", byteStart);
        if (eqPos != string::npos && (int)eqPos < cleanByteEnd)
        {
            setcolor(window, UIYELLOW);
            string sEq = fullStr.substr(byteStart, eqPos - byteStart + 1);
            mvwprintw(window, currentY, x, "%s", sEq.c_str());
        }

        // Секции [header]
        string trimmed = trim(cleanStr);
        if (!trimmed.empty() && trimmed[0] == '[' && trimmed.find(']') != string::npos)
        {
            setcolor(window, UIGREEN);
            string sSec = fullStr.substr(byteStart, cleanByteEnd - byteStart);
            mvwprintw(window, currentY, x, "%s", sSec.c_str());
        }
    }

    if (currentY < y + m_viewH)
    {
        setcolor(window, color1);
        mvwprintw(window, currentY, x, "%*s", m_viewW, "");
    }

    setcolor(window, color1);
    mvwvline(window, y, x + m_viewW, ACS_VLINE, m_viewH);

    if (m_lineCount > m_viewH)
    {
        double k = (double)m_viewH / m_lineCount;
        int barSize = (int)Round(m_viewH * k); if (barSize == 0) barSize = 1;
        int barPos = (int)Round(m_scrollOffset.y * k);
        if (barPos + barSize >= m_viewH) barPos = m_viewH - barSize;
        setcolor(window, -color1);
        mvwvline(window, y + barPos, x + m_viewW, ' ', barSize);
    }
    cursor();
}

//-------------------------------------------------------------------------------------------

void TUIMemo::cursor()
{
    FUNCTION_TRACE
    wmove(window, y + m_cursorPos.y, x + m_cursorPos.x);
    curs_on();
}

bool TUIMemo::event(int inputKey, char* scanCodes)
{
    FUNCTION_TRACE
    if (ESCAPE(inputKey)) return true;
    if ((inputKey != KEY_F(8)) && (inputKey >= KEY_F(0)) && (inputKey < KEY_F(64))) return true;

    switch (inputKey)
    {
        case KEY_UP:
            if (IsFirstLine()) return false;
            --m_currentIterator; --m_cursorPos.y;
            break;
        case KEY_DOWN:
            if (IsLastLine()) return false;
            ++m_currentIterator; ++m_cursorPos.y;
            break;
        case KEY_PPAGE:
            if (IsFirstLine()) return false;
            for (int i = 0; (i < m_viewH - 1) && !IsFirstLine(); ++i) {
                --m_currentIterator; --m_drawIterator; --m_scrollOffset.y;
            }
            break;
        case KEY_NPAGE:
            if (IsLastLine()) return false;
            for (int i = 0; (i < m_viewH - 1) && !IsLastLine(); ++i) {
                ++m_currentIterator; ++m_drawIterator; ++m_scrollOffset.y;
            }
            break;
        case KEY_LEFT:
            if (IsFirstLine() && IsAtLineStart()) return false;
            if (IsAtLineStart()) {
                --m_currentIterator; --m_cursorPos.y;
                m_charPos = GetLineLen(m_currentIterator);
            } else --m_charPos;
            m_desiredCharPos = m_charPos;
            break;
        case KEY_RIGHT:
            if (IsLastLine() && IsAtLineEnd()) return false;
            if (IsAtLineEnd()) {
                ++m_currentIterator; ++m_cursorPos.y;
                m_charPos = 0;
            } else ++m_charPos;
            m_desiredCharPos = m_charPos;
            break;
        case KEY_HOME: m_desiredCharPos = m_charPos = 0; break;
        case KEY_END : m_desiredCharPos = m_charPos = GetLineLen(m_currentIterator); break;
        case 10:
        case 13:
        {
            bool wasFirst = IsFirstLine();
            int splitIdx = utf8indexsymbols(*m_currentIterator, m_charPos);
            m_lines.insert(m_currentIterator, m_currentIterator->substr(0, splitIdx));
            m_lineCount++;
            *m_currentIterator = m_currentIterator->substr(splitIdx);
            if (wasFirst) m_drawIterator = m_lines.begin();
            ++m_cursorPos.y; m_desiredCharPos = m_charPos = 0;
        } break;
        case KEY_BACKSPACE:
            if (IsFirstLine() && IsAtLineStart()) return false;
            if (IsAtLineStart()) {
                TLineIterator prev = m_currentIterator; --prev;
                int oldLen = GetLineLen(prev);
                prev->append(*m_currentIterator);
                m_currentIterator = m_lines.erase(m_currentIterator);
                m_lineCount--; --m_currentIterator; --m_cursorPos.y;
                m_desiredCharPos = m_charPos = oldLen;
            } else {
                int i1 = utf8indexsymbols(*m_currentIterator, m_charPos - 1);
                int i2 = utf8indexsymbols(*m_currentIterator, m_charPos);
                m_currentIterator->erase(i1, i2 - i1);
                --m_charPos; m_desiredCharPos = m_charPos;
            }
            break;
        case KEY_DC:
            if (IsLastLine() && IsAtLineEnd()) return false;
            if (IsAtLineEnd()) {
                TLineIterator next = m_currentIterator; ++next;
                m_currentIterator->append(*next);
                m_lines.erase(next); m_lineCount--;
            } else {
                int i1 = utf8indexsymbols(*m_currentIterator, m_charPos);
                int i2 = utf8indexsymbols(*m_currentIterator, m_charPos + 1);
                m_currentIterator->erase(i1, i2 - i1);
            }
            break;
        case KEY_F(8):
            if (IsLastLine() && IsAtLineEnd() && m_lineCount == 1) m_currentIterator->clear();
            else if (IsLastLine()) m_currentIterator->clear();
            else {
                TLineIterator next = m_currentIterator; ++next;
                m_lines.erase(m_currentIterator); m_lineCount--;
                m_currentIterator = next;
                if (IsFirstLine()) m_drawIterator = m_currentIterator;
            }
            m_charPos = m_desiredCharPos = 0;
            break;
        default:
        {
            string inputStr = scanCodes;
            int inputLen = utf8length(inputStr);
            if (inputLen <= 0) { errno = 0; return false; }
            m_currentIterator->insert(utf8indexsymbols(*m_currentIterator, m_charPos), inputStr);
            m_charPos += inputLen; m_desiredCharPos = m_charPos;
        }
    }

    ApplyScrolling();
    draw();
    uirefresh();
    return false;
}

void TUIMemo::ApplyScrolling()
{
    if (m_lineCount == 0) return;
    int currentLineLen = GetLineLen(m_currentIterator);
    m_charPos = (m_desiredCharPos <= currentLineLen) ? m_desiredCharPos : currentLineLen;
    if (m_charPos >= m_scrollOffset.x + m_viewW)  m_scrollOffset.x = m_charPos - m_viewW + 1;
    if (m_charPos < m_scrollOffset.x + 20)        m_scrollOffset.x = m_charPos - 20;
    if (m_scrollOffset.x < 0) m_scrollOffset.x = 0;
    m_cursorPos.x = m_charPos - m_scrollOffset.x;
    if (m_scrollOffset.y < 0) { m_drawIterator = m_lines.begin(); m_cursorPos.y += m_scrollOffset.y; m_scrollOffset.y = 0; }
    while ((m_scrollOffset.y + m_viewH > m_lineCount) && (m_drawIterator != m_lines.begin())) { --m_drawIterator; --m_scrollOffset.y; ++m_cursorPos.y; }
    while (m_cursorPos.y < 0) { ++m_cursorPos.y; --m_scrollOffset.y; --m_drawIterator; }
    while (m_cursorPos.y >= m_viewH) { --m_cursorPos.y; ++m_scrollOffset.y; ++m_drawIterator; }
}

uint32_t TUIMemo::CalculateCrc32()
{
    uint32_t crc = 0;
    for (TLineIterator it = m_lines.begin(); it != m_lines.end(); ++it) {
        crc = fcrc32(crc, it->c_str(), it->size());
        crc = fcrc32(crc, "\n", 1);
    }
    return crc;
}

bool TUIMemo::LoadFromFile(std::string filename)
{
    m_lines.clear(); m_lineCount = 0;
    ifstream ifs(filename.c_str());
    if (!ifs.is_open()) return false;
    string line;
    while (getline(ifs, line)) { m_lines.push_back(removecolor(line)); m_lineCount++; }
    ifs.close();
    if (m_lines.empty()) { m_lines.push_back(""); m_lineCount = 1; }
    m_drawIterator = m_currentIterator = m_lines.begin();
    m_cursorPos.set(0, 0); m_scrollOffset.set(0, 0); m_charPos = m_desiredCharPos = 0;
    m_originalCrc = CalculateCrc32();
    setcolor(window, color1);
    for (int i = 0; i < h; i++) mvwprintw(window, y + i, x, "%*s", w, "");
    return true;
}

bool TUIMemo::SaveToFile(std::string filename)
{
    ifstream src(filename.c_str(), ios::binary);
    if (src.is_open()) {
        string backupName = filename + "~";
        ofstream dst(backupName.c_str(), ios::binary);
        dst << src.rdbuf(); dst.close(); src.close();
    }
    ofstream ofs(filename.c_str());
    if (!ofs.is_open()) return false;
    for (TLineIterator it = m_lines.begin(); it != m_lines.end(); ++it) {
        ofs << *it;
        TLineIterator next = it; ++next;
        if (next != m_lines.end()) ofs << "\n";
    }
    ofs.close();
    m_originalCrc = CalculateCrc32();
    return true;
}
