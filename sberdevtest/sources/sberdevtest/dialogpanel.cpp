#include <string>
#include "debugsystem.h"
#include "main.h"
#include "ui.h"
#include "menupanel.h"
#include "dialogpanel.h"
#include "test.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TDialogPanel* DialogPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ DIALOGPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TDialogPanel::TDialogPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;
    m_messageText = "";
    m_inputField  = NULL;
    m_activeButtons.clear();
}

TDialogPanel::~TDialogPanel( )
{
    FUNCTION_TRACE
    m_activeButtons.clear();
}

//-------------------------------------------------------------------------------------------

void TDialogPanel::resize( )
{
    FUNCTION_TRACE
    // Центрируем панель по экрану
    doresize( (COLS - w) / 2, (LINES - h) / 2, w, h );
}

void TDialogPanel::draw( )
{
    FUNCTION_TRACE
    setcolor( window, color );
    box( window, 0, 0 );
    drawlineH( window, 0, h - 5, w ); // Линия над кнопками

    if( !m_windowTitle.empty() )
        mvwprintw( window, 0, (w - utf8length(m_windowTitle)) / 2, " %s ", m_windowTitle.c_str() );

    if( m_currentMode == MODE_MESSAGE )
        mvwprintw( window, 2, (w - utf8length(m_messageText)) / 2, "%s", m_messageText.c_str() );

    if( m_currentMode == MODE_INPUT )
        mvwprintw( window, 2, 3, "%s", m_messageText.c_str() );

    drawobjects( );
}

//-------------------------------------------------------------------------------------------
bool TDialogPanel::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE
    if( ESCAPE(inputKey) )
    {
        SyncHide( );
        if( m_shouldCloseMenuOnEscape ) MenuPanel->HandleEscape( );
    }
    return true;
}

//-------------------------------------------------------------------------------------------

/**
 * @brief Разбор конфигурационной строки и отрисовка элементов диалога
 */
void TDialogPanel::SyncShow( )
{
    FUNCTION_TRACE
    char *cursor, *start = (char*)m_setupString.c_str();
    int targetW = 0, targetH = 0;

    // 1. Парсим размеры W,H
    sscanf( start, "%d,%d", &targetW, &targetH );

    // 2. Парсим Заголовок {Title}
    cursor = strchr( start, '{' ); if( !cursor ) return;
    char* end = strchr( cursor, '}' ); if( !end ) return;
    m_windowTitle = string( cursor + 1, end );

    // 3. Парсим Текст сообщения {Caption}
    cursor = strchr( end, '{' ); if( !cursor ) return;
    end = strchr( cursor, '}' ); if( !end ) return;
    m_messageText = string( cursor + 1, end );

    // 4. Парсим Кнопки {Ok|Cancel}
    cursor = strchr( end, '{' ); if( !cursor ) return;
    end = strchr( cursor, '}' ); if( !end ) return;
    string buttonConfig = string( cursor + 1, end );

    // 5. Парсим Кнопку по умолчанию {Ok}
    cursor = strchr( end, '{' ); if( !cursor ) return;
    end = strchr( cursor, '}' ); if( !end ) return;
    string defaultButtonText = string( cursor + 1, end );

    // Применяем новые размеры
    doresize( (COLS - targetW) / 2, (LINES - targetH) / 2, targetW, targetH );

    // Очистка старых объектов
    if( m_inputField ) { delete m_inputField; m_inputField = NULL; }
    for( auto btn : m_activeButtons ) delete btn;
    m_activeButtons.clear();

    // Расчет размеров кнопок
    int btnWidth = 10;
    int btnSpacing = 2;
    int btnCount = 0;
    char* btnNamePtr = (char*)buttonConfig.c_str();

    // Сначала считаем ширину
    while( true ) {
        char* delimiter = strchr( btnNamePtr, '|' );
        string s = delimiter ? string(btnNamePtr, delimiter) : string(btnNamePtr);
        int len = utf8length( s ) + 2;
        if( len > btnWidth ) btnWidth = len;
        btnCount++;
        if( !delimiter ) break;
        btnNamePtr = delimiter + 1;
    }

    // Создаем кнопки
    int currentX = ( w - btnWidth * btnCount - btnSpacing * (btnCount - 1) ) / 2;
    btnNamePtr = (char*)buttonConfig.c_str();
    while( true ) {
        char* delimiter = strchr( btnNamePtr, '|' );
        string s = delimiter ? string(btnNamePtr, delimiter) : string(btnNamePtr);

        TUIButton* newBtn = new TUIButton( this, "", currentX, h - 3, btnWidth, 1,
                                          -UIBLACK, UIGREEN, s, UIC, m_onButtonClickCallback );
        m_activeButtons.push_back( newBtn );

        if( !delimiter ) break;
        btnNamePtr = delimiter + 1;
        currentX += btnWidth + btnSpacing;
    }

    // Режим работы
    if( m_currentMode == MODE_MESSAGE )
    {
        bool focused = false;
        for( auto btn : m_activeButtons ) {
            if( btn->text == defaultButtonText ) { setfocus( btn ); focused = true; break; }
        }
        if( !focused && !m_activeButtons.empty() ) setfocus( m_activeButtons.front() );
    }
    else if( m_currentMode == MODE_INPUT )
    {
        m_inputField = new TUIEdit( this, "edit", 3, 3, targetW - 6, 1, -UIBLACK, UIGREEN, defaultButtonText );
        setfocus( m_inputField );
    }

    ui->tablock( true );
    ui->setfocus( this );
}

void TDialogPanel::SyncHide( )
{
    FUNCTION_TRACE
    if( !visible ) return;
    hide( );
    ui->tablock( false );
    ui->setfocusprev( );
}

//-------------------------------------------------------------------------------------------

void TDialogPanel::ShowMessage( int windowColor, const std::string config, void (*onBtnClick)( TUIButton* btn ), bool escapeToMenu )
{
    FUNCTION_TRACE
    m_currentMode = MODE_MESSAGE;
    this->color = windowColor;
    this->m_setupString = trim( config );
    this->m_onButtonClickCallback = onBtnClick;
    this->m_shouldCloseMenuOnEscape = escapeToMenu;

    if( pthread_self( ) == g_DebugSystem.mainthread ) SyncShow( );
    else UI->DoLock( DIALOG_SHOW );
}

void TDialogPanel::ShowInput( int windowColor, const std::string config, void (*onBtnClick)( TUIButton* btn ), bool escapeToMenu )
{
    FUNCTION_TRACE
    m_currentMode = MODE_INPUT;
    this->color = windowColor;
    this->m_setupString = trim( config );
    this->m_onButtonClickCallback = onBtnClick;
    this->m_shouldCloseMenuOnEscape = escapeToMenu;

    if( pthread_self( ) == g_DebugSystem.mainthread ) SyncShow( );
    else UI->DoLock( DIALOG_SHOW );
}

void TDialogPanel::Close( )
{
    FUNCTION_TRACE
    if( pthread_self( ) == g_DebugSystem.mainthread ) SyncHide( );
    else UI->DoLock( DIALOG_HIDE );
}
