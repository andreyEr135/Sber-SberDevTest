#ifndef uimemoH
#define uimemoH

#include <list>
#include <iterator>
#include <string>
#include "crc32.h"
#include "ui.h"

//-------------------------------------------------------------------------------------------

/**
 * @brief Структура для хранения координат (курсор, смещение)
 */
struct TCoordinate
{
    int x, y;
    void set(int _x, int _y) { x = _x; y = _y; }
};

/**
 * @brief Виджет многострочного текстового редактора
 */
class TUIMemo : public TUIObject
{
  protected:
    typedef std::list<std::string> TLineList;
    typedef TLineList::iterator    TLineIterator;

    TLineList     m_lines;          ///< Список строк текста
    int           m_lineCount;      ///< Общее количество строк
    uint32_t      m_originalCrc;    ///< CRC32 на момент открытия/сохранения

    int           m_viewW;          ///< Ширина области текста
    int           m_viewH;          ///< Высота области текста

    TLineIterator m_drawIterator;   ///< Итератор строки, с которой начинается отрисовка (верхняя видимая)
    TLineIterator m_currentIterator;///< Итератор текущей строки, где находится курсор

    int           m_charPos;        ///< Позиция курсора в символах внутри строки
    int           m_desiredCharPos; ///< Желаемая позиция (для сохранения X при перемещении UP/DOWN)

    TCoordinate   m_cursorPos;      ///< Координаты курсора относительно окна
    TCoordinate   m_scrollOffset;   ///< Смещение прокрутки (x - горизонтальное, y - вертикальное)

    // Вспомогательные методы
    int  GetLineLen(TLineIterator it) { return utf8length(*it); }
    bool IsFirstLine() { return (m_currentIterator == m_lines.begin()); }
    bool IsLastLine()  { return (m_currentIterator == --m_lines.end()); }
    bool IsAtLineStart() { return (m_charPos == 0); }
    bool IsAtLineEnd()   { return (m_charPos == GetLineLen(m_currentIterator)); }

    void     ApplyScrolling();
    uint32_t CalculateCrc32();

    virtual bool event(int inputKey, char* scanCodes);

  public:
    TUIMemo(const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color);
    virtual ~TUIMemo();

    virtual void resize(int x, int y, int w, int h);
    virtual void draw();
    virtual void cursor();

    bool LoadFromFile(std::string filename);
    bool SaveToFile(std::string filename);
    bool IsModified() { return (m_originalCrc != CalculateCrc32()); }
};

//-------------------------------------------------------------------------------------------
#endif
