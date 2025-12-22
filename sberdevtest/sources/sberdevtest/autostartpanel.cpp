#include "debugsystem.h"
#include "main.h"
#include "test.h"
#include "ui.h"
#include "backpanel.h"
#include "hellopanel.h"
#include "menupanel.h"
#include "logpanel.h"
#include "sensorpanel.h"
#include "autostartpanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TAutoStartPanel* AutoStartPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ AUTOSTARTPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TAutoStartPanel::TAutoStartPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;

    new TUILabel( this, "", 0, 1, w, 0, color, "Автозапуск комплексного теста", UIC );

    m_iniFileLabel   = new TUILabel( this, "", 0, 3, w, 0, color, "", UIC );
    m_commentLabel   = new TUILabel( this, "", 0, 4, w, 0, color, "", UIC );
    m_countDownLabel = new TUILabel( this, "", 0, 6, w, 0, color, "через 10 сек", UIC );

    m_remainingSeconds = 10;
}

//-------------------------------------------------------------------------------------------

TAutoStartPanel::~TAutoStartPanel( )
{
    FUNCTION_TRACE
}

//-------------------------------------------------------------------------------------------

void TAutoStartPanel::resize( )
{
    FUNCTION_TRACE
    doresize( (COLS - w) / 2, (LINES - h) / 2, w, h );
}

//-------------------------------------------------------------------------------------------

void TAutoStartPanel::draw( )
{
    FUNCTION_TRACE
    drawobjects( );
}

//-------------------------------------------------------------------------------------------

bool TAutoStartPanel::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE

    // Если нажат Enter - запускаем тест немедленно
    if( ENTER(inputKey) )
    {
        StopTimer( m_autoStartTimer );

        this->hide( );
        BackPanel->ShowMainLayout( );
        MenuPanel->ShowComplexMenu( );
        UI->draw( );

        Test->InitTests( );
        OnMenuButtonClick( MenuPanel->m_complexStartBtn );
    }
    else // Любая другая клавиша отменяет автозапуск
    {
        StopTimer( m_autoStartTimer );
        this->hide( );

        MenuPanel->m_customCyclesEdit->setint( 1 );
        BackPanel->HideMainLayout( );
        HelloPanel->ShowPanel( );
    }
    return true;
}

//-------------------------------------------------------------------------------------------

/**
 * @brief Глобальный коллбэк таймера
 */
void OnAutoStartTimerTrigger( sigval sv )
{
    DTHRD("{AutoStartTimer}")
    // Посылаем сигнал UI потоку обновить панель
    UI->DoLock( TIMER_AUTOSTART );
}

//-------------------------------------------------------------------------------------------

void TAutoStartPanel::ShowPanel( )
{
    FUNCTION_TRACE
    BackPanel->show( );

    try
    {
        string lastConfig = Test->GetLastIni( );
        if( lastConfig.empty() ) throw 1;
        if( !Test->LoadIni( lastConfig ) ) throw 2;

        int loopCount = Test->ini->ReadInt( "cfg", "loop_count" );
        MenuPanel->m_customCyclesEdit->setint( loopCount );

        BackPanel->StatusLabel->text = "Нажмите Enter для старта или любую клавишу для отмены";

        m_iniFileLabel->settext( Test->iniitem->name );
        m_commentLabel->settext( Test->iniitem->comment );
        m_countDownLabel->text = stringformat( "через %d сек", m_remainingSeconds );

        curs_off( );
        UI->setfocus( this );

        // Запуск секундного таймера
        if( StartTimerNewThread( &m_autoStartTimer, 1, 0, &OnAutoStartTimerTrigger, 0 ) != 0 )
            Error( "Failed to start AutoStartTimer\n" );
    }
    catch( int errorCode )
    {
        this->hide( );
        HelloPanel->ShowPanel( );
    }
}

//-------------------------------------------------------------------------------------------

/**
 * @brief Вызывается каждую секунду по таймеру
 */
void TAutoStartPanel::UpdateTick( )
{
    FUNCTION_TRACE

    m_remainingSeconds--;
    m_countDownLabel->text = stringformat( "через %d сек", m_remainingSeconds );

    draw( );
    uirefresh( );

    // Если время вышло - автоматически запускаем тест
    if( m_remainingSeconds <= 0 )
    {
        StopTimer( m_autoStartTimer );

        this->hide( );
        BackPanel->ShowMainLayout( );
        MenuPanel->ShowComplexMenu( );
        UI->draw( );

        Test->InitTests( );
        OnMenuButtonClick( MenuPanel->m_complexStartBtn );
    }
}

//-------------------------------------------------------------------------------------------

void TAutoStartPanel::ResetCountdown()
{
    m_remainingSeconds = 10;
}

//-------------------------------------------------------------------------------------------
