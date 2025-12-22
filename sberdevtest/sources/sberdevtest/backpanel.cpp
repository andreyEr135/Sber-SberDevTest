#include "debugsystem.h"
#include "main.h"
#include "test.h"
#include "ui.h"
#include "hellopanel.h"
#include "menupanel.h"
#include "logpanel.h"
#include "sensorpanel.h"
#include "programpanel.h"
#include "editpanel.h"
#include "backpanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TBackPanel* BackPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ BACKPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TBackPanel::TBackPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    TitleLabel       = new TUILabel( this, "TitleLabel",  0, 0, 0, 0, -color, "", UIL );
    DeviceLabel      = new TUILabel( this, "DeviceLabel", 0, 0, 0, 0, -color, "", UIL );
    StatusLabel      = new TUILabel( this, "StatusLabel", 0, 0, 0, 0, -color, "", UIL );
    TestControlLabel = nullptr; // Инициализация на случай использования

    TitleLabel->settext( " " + string(GetVersionStr(VERSION, false)) );
}

//-------------------------------------------------------------------------------------------

TBackPanel::~TBackPanel( )
{
    FUNCTION_TRACE
}

//-------------------------------------------------------------------------------------------

void TBackPanel::resize( )
{
    FUNCTION_TRACE
    doresize( 0, 0, COLS, LINES );
    TitleLabel->resize( 0, 0, COLS, 1 );
    StatusLabel->resize( 0, h - 1, COLS, 1 );
}

//-------------------------------------------------------------------------------------------

void TBackPanel::draw( )
{
    FUNCTION_TRACE
    drawobjects( );
}

//-------------------------------------------------------------------------------------------

void TBackPanel::ShowMainLayout( )
{
    FUNCTION_TRACE
    StatusLabel->text = Test->iniitem->name + " | " + Test->iniitem->comment;

    MenuPanel->ShowMainMenu( );
    LogPanel->show( );
    SensorPanel->show( );
}

//-------------------------------------------------------------------------------------------

void TBackPanel::HideMainLayout( )
{
    FUNCTION_TRACE
    if (!(g_DebugSystem.conf->ReadInt("Main", "block")))
        StatusLabel->text = "F4-Редактировать | Esc-Закрыть";
    else
        StatusLabel->text = "Esc-Закрыть";

    EditPanel->hide( );
    MenuPanel->hide( );
    LogPanel->hide( );
    SensorPanel->hide( );
}

//-------------------------------------------------------------------------------------------

void TBackPanel::ShowProgramLayout( )
{
    FUNCTION_TRACE
    StatusLabel->text = "Программирование устройства";
    ProgramPanel->ShowPanel();
}

//-------------------------------------------------------------------------------------------

void TBackPanel::HideProgramLayout( )
{
    FUNCTION_TRACE
    if (!(g_DebugSystem.conf->ReadInt("Main", "block")))
        StatusLabel->text = "F4-Редактировать | Esc-Закрыть";
    else
        StatusLabel->text = "Esc-Закрыть";
}

//-------------------------------------------------------------------------------------------

void TBackPanel::StartComplexTest( )
{
    FUNCTION_TRACE
    string lastConfig = Test->GetLastIni( );
    if( lastConfig.empty() ) return;
    if( !Test->LoadIni( lastConfig ) ) return;

    Test->InitTests( );

    int cycleCount = Test->ini->ReadInt( "cfg", "loop_count" );
    MenuPanel->m_complexCyclesEdit->setint( cycleCount );

    HelloPanel->HidePanel( );
    this->ShowMainLayout( );
    MenuPanel->ShowComplexMenu( );

    UI->draw( );

    // Имитация нажатия кнопки старта
    OnMenuButtonClick( MenuPanel->m_complexStartBtn );
}

//-------------------------------------------------------------------------------------------

void TBackPanel::StopComplexTest( )
{
    FUNCTION_TRACE
    Test->Stop();
}

//-------------------------------------------------------------------------------------------
