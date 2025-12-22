#include "debugsystem.h"
#include "main.h"
#include "test.h"
#include "ui.h"
#include "backpanel.h"
#include "dialogpanel.h"
#include "hellopanel.h"
#include "menupanel.h"
#include "logpanel.h"
#include "editpanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TMenuPanel* MenuPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ MENUPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TMenuPanel::TMenuPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;
    m_lastFocusedCustomBtn = NULL;

    // Основные кнопки
    m_complexBtn  = new TUIButton( this, "ComplexBtn", 1, 1, w-2, 3, -color, UIGREEN, "Комплексный тест", UIC, &OnMenuButtonClick );
    m_customBtn   = new TUIButton( this, "CustomBtn",  1, 5, w-2, 3, -color, UIGREEN, "Выборочный тест", UIC, &OnMenuButtonClick );
    m_settingsBtn = new TUIButton( this, "SettingBtn", 1, 9, w-2, 3, -color, UIGREEN, "Настройки", UIC, &OnMenuButtonClick );
    m_scannerBtn  = new TUIButton( this, "ScannerBtn", 1, 13, w-2, 3, -color, UIGREEN, "", UIC, &OnMenuButtonClick );
    m_backBtn     = new TUIButton( this, "ReturnBtn",  1, 2, w-2, 1, -color, UIGREEN, "Назад", UIC, &OnMenuButtonClick );

    // Индикация выполнения
    m_cyclesLabel    = new TUILabel ( this, "CycleLabel", 1, 4, 0, 1, color, "Циклов :", UIL );
    m_runStatusLabel = new TUILabel ( this, "RunLabel",   1, 5, 0, 1, color, "1 / 1", UIL );
    m_stopBtn        = new TUIButton( this, "StopBtn",    1, 7, w-2, 3, -color, UIRED, "Стоп", UIC, &OnMenuButtonClick );

    // Настройка комплексного теста
    m_complexHeader     = new TUILabel ( this, "ComplexLabel", 0, 0, w, 1, UIBLUE, "Комплексный тест", UIC );
    m_complexCyclesEdit = new TUIEdit  ( this, "ComplexCyclesEdit", 1, 5, w-2, 1, -color, UIGREEN, "1" );
    m_complexStartBtn   = new TUIButton( this, "ComplexStartBtn", 1, 7, w-2, 3, -color, UIGREEN, "Старт", UIC, &OnMenuButtonClick );

    // Настройка выборочного теста
    m_customHeader      = new TUILabel ( this, "CustomLabel", 0, 0, w, 1, UIBLUE, "Выборочный тест", UIC );
    m_customCyclesEdit  = new TUIEdit  ( this, "CustomCyclesEdit", 1, 5, w-2, 1, -color, UIGREEN, "1" );
}

TMenuPanel::~TMenuPanel( )
{
    FUNCTION_TRACE
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::resize( )
{
    FUNCTION_TRACE
    doresize( 0, 1, w, LINES - 2 );
}

void TMenuPanel::draw( )
{
    FUNCTION_TRACE
    drawobjects( );
}

//-------------------------------------------------------------------------------------------

bool TMenuPanel::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE

    if( ESCAPE(inputKey) ) HandleEscape( );
    if( inputKey == KEY_DC ) Test->Kill( ); // Delete для экстренной остановки

    switch( inputKey )
    {
        case 'i': // Информационная клавиша
            if( (m_currentView == VIEW_CUSTOM) && focusobject && (focusobject->name == "TestBtn") )
                Test->InfoTest( focusobject->text );
            else if( focusobject && (focusobject->name == "ComplexBtn") )
                /* Показать инфо о комплексном тесте */;
            else if( focusobject && (focusobject->name == "CustomBtn") )
                Test->InfoTests( );
            break;

        case KEY_F(3): // Быстрый переход в настройки
            if (!(g_DebugSystem.conf->ReadInt("Main", "block")))
                OnMenuButtonClick( m_settingsBtn );
            break;

        case KEY_PPAGE:
            LogPanel->ScrollPageUp( );
            break;

        case KEY_NPAGE:
            LogPanel->ScrollPageDown( );
            break;
    }

    return true;
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::hideAllObjects( )
{
    FUNCTION_TRACE
    FOR_EACH_ITER( TUIObjectList, listobject, it )
    {
        ((TUIObject*)(*it))->visible = false;
    }
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::ShowMainMenu( )
{
    FUNCTION_TRACE
    hideAllObjects( );

    m_complexBtn->visible = true;
    m_customBtn->visible  = true;

    bool isBlocked = (g_DebugSystem.conf->ReadInt("Main", "block") != 0);
    if (!isBlocked)
    {
        m_scannerBtn->visible = (m_scannerBtn->text != "");
        m_settingsBtn->visible = true;
    }

    TUIObject* focusTarget = m_complexBtn;
    if( m_currentView == VIEW_COMPLEX ) focusTarget = m_complexBtn;
    if( m_currentView == VIEW_CUSTOM  ) focusTarget = m_customBtn;

    clear( 0, 0, w, h );
    showfocused( focusTarget );
    m_currentView = VIEW_MAIN;
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::ShowComplexMenu( )
{
    FUNCTION_TRACE
    hideAllObjects( );

    m_backBtn->visible           = true;
    m_complexHeader->visible     = true;
    m_cyclesLabel->visible       = true;
    m_complexCyclesEdit->visible = true;
    m_complexStartBtn->visible   = true;

    clear( 0, 0, w, h );
    showfocused( (m_currentView == VIEW_RUN_COMPLEX) ? m_complexStartBtn : m_backBtn );
    m_currentView = VIEW_COMPLEX;
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::ShowCustomMenu( )
{
    FUNCTION_TRACE
    hideAllObjects( );

    m_backBtn->visible          = true;
    m_customHeader->visible     = true;
    m_cyclesLabel->visible      = true;
    m_customCyclesEdit->visible = true;

    Test->TestButtonsVisible( ); // Метод Test отображает кнопки тестов

    clear( 0, 0, w, h );
    showfocused( (m_currentView == VIEW_RUN_CUSTOM) ? m_lastFocusedCustomBtn : m_backBtn );
    m_currentView = VIEW_CUSTOM;
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::ShowRunningState( )
{
    FUNCTION_TRACE
    hideAllObjects( );

    m_cyclesLabel->visible    = true;
    m_runStatusLabel->visible = true;
    m_stopBtn->visible        = true;

    if( m_currentView == VIEW_COMPLEX ) m_currentView = VIEW_RUN_COMPLEX;
    if( m_currentView == VIEW_CUSTOM  ) m_currentView = VIEW_RUN_CUSTOM;

    if( m_currentView == VIEW_RUN_COMPLEX ) m_complexHeader->visible = true;
    if( m_currentView == VIEW_RUN_CUSTOM  ) m_customHeader->visible  = true;

    clear( 0, 0, w, h );
    showfocused( m_stopBtn );
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::HandleEscape( )
{
    FUNCTION_TRACE
    if( Test->working )
    {
        OnMenuButtonClick( m_stopBtn );
    }

    switch( m_currentView )
    {
        case VIEW_MAIN:    HelloPanel->ShowPanel( ); break;
        case VIEW_COMPLEX: ShowMainMenu( ); break;
        case VIEW_CUSTOM:  ShowMainMenu( ); break;
        case VIEW_RUN_SINGLE: m_currentView = VIEW_CUSTOM; break;
        default: break;
    }
}

//-------------------------------------------------------------------------------------------

void TMenuPanel::UpdateState( const std::string& command )
{
    FUNCTION_TRACE
    if( command == "complex" ) ShowComplexMenu( );
    else if( command == "custom"  ) ShowCustomMenu( );
    else if( command == "onerun"  ) m_currentView = VIEW_RUN_SINGLE;
    else if( command == "onestop" ) m_currentView = VIEW_CUSTOM;
    else
    {
        m_runStatusLabel->text = command;
        ShowRunningState( );
    }
}

//-------------------------------------------------------------------------------------------
// Вспомогательные функции (коллбэки редактирования)
//-------------------------------------------------------------------------------------------

void OnIniEditExit( std::string file, bool changed )
{
    FUNCTION_TRACE
    if( changed )
    {
        Test->SearchInis( );
        Test->SetLastIni( file );
        Test->LoadIni( file );
        Test->InitTests( );
    }
    BackPanel->ShowMainLayout( );
    UI->draw( );
}

void OnScannerEditExit( std::string file, bool changed )
{
    FUNCTION_TRACE
    if( changed )
    {
        Test->LoadIni( Test->ininame );
        Test->InitTests( );
    }
    BackPanel->ShowMainLayout( );
    UI->draw( );
}

//-------------------------------------------------------------------------------------------
// Обработка нажатий кнопок меню
//-------------------------------------------------------------------------------------------

void OnMenuButtonClick( TUIButton* btn )
{
    FUNCTION_TRACE
    if( btn == MenuPanel->m_complexBtn )
        MenuPanel->ShowComplexMenu( );
    else if( btn == MenuPanel->m_customBtn )
        MenuPanel->ShowCustomMenu( );
    else if( btn == MenuPanel->m_settingsBtn )
        EditPanel->ShowFile( Test->inidir + "/" + Test->iniitem->name, &OnIniEditExit );
    else if( btn == MenuPanel->m_scannerBtn )
        EditPanel->ShowFile( getpath(Test->scanner_run) + "/" + getfile(Test->scanner_conf), &OnScannerEditExit );
    else if( btn == MenuPanel->m_backBtn )
        MenuPanel->ShowMainMenu( );
    else if( btn == MenuPanel->m_complexStartBtn )
        Test->StartComplex( MenuPanel->m_complexCyclesEdit->getint() );
    else if( btn == MenuPanel->m_stopBtn )
        Test->Stop( );
}

//-------------------------------------------------------------------------------------------

void OnTestButtonClick( TUIButton* btn )
{
    FUNCTION_TRACE
    MenuPanel->m_lastFocusedCustomBtn = btn;
    string testName = trim( btn->text );
    Test->StartCustom( testName, MenuPanel->m_customCyclesEdit->getint() );
}
