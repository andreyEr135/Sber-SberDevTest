#include "debugsystem.h"
#include "main.h"
#include "uitestbtn.h"

using namespace std;

//-------------------------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UITESTBTN_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TUITestButton::TUITestButton( const TUIPanel* parentPanel,
                               const std::string& name,
                               int x, int y, int w, int h,
                               int colorNormal,
                               int colorFocused,
                               const std::string& label,
                               int alignment,
                               void (*OnPress)( TUIButton* btn ) )
    : TUIButton( parentPanel, name, x, y, w, h, colorNormal, colorFocused, label, alignment, OnPress )
{
    FUNCTION_TRACE
    Debug( "Создана тестовая кнопка: %s\n", this->name.c_str() );
}

TUITestButton::~TUITestButton( )
{
    FUNCTION_TRACE
    Debug( "Удалена тестовая кнопка: %s\n", this->name.c_str() );
}

//-------------------------------------------------------------------------------------------

bool TUITestButton::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE
    // Здесь можно добавить специфичную обработку клавиш для кнопок тестов
    // Если ничего специфичного нет, возвращаем true, чтобы событие не провалилось дальше
    return true;
}

//-------------------------------------------------------------------------------------------

void TUITestButton::draw( )
{
    FUNCTION_TRACE
    if( !visible ) return;

    // Определяем цвет в зависимости от фокуса
    bool isCurrentlyFocused = ( focused && uipanel->isfocused() );
    setcolor( window, isCurrentlyFocused ? color2 : color1 );

    // Очистка (заливка) области кнопки
    for( int i = 0; i < h; i++ )
    {
        if( mvwprintw( window, y + i, x + 1, "%*s", w - 2, "" ) == ERR )
            Error( "Ошибка mvwprintw при очистке кнопки %s\n", name.c_str() );
    }

    // Отрисовка основного текста (центрирование или выравнивание)
    int labelY = y + (h - 1) / 2;
    if( mvwprintw( window, labelY, xalign + 1, "%s", text.c_str() ) == ERR )
        Error( "Ошибка mvwprintw при выводе текста кнопки %s\n", name.c_str() );

    // Отрисовка специфических маркеров тестовой кнопки:
    // 'c' бирюзовым цветом слева
    setcolor( window, UIcyan );
    if( mvwprintw( window, labelY, xalign, "c" ) == ERR )
        Error( "Ошибка mvwprintw (marker 'c')\n" );

    // 's' красным цветом справа
    setcolor( window, UIRED );
    if( mvwprintw( window, labelY, x + w - 2, "s" ) == ERR )
        Error( "Ошибка mvwprintw (marker 's')\n" );
}
