#include <string>
#include "debugsystem.h"
#include "main.h"
#include "ui.h"
#include "hellopanel.h"
#include "menupanel.h"
#include "sensorpanel.h"
#include "logpanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TLogPanel* LogPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ LOGPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TLogPanel::TLogPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;
    m_logLines.clear();
    m_logLines.push_back( "" );

    m_currentLogSize = 1;
    m_lastLineIt     = m_logLines.begin();
    m_scrollLineIt   = m_lastLineIt;
    m_scrollPosition = 0;

    m_maxLogSize     = g_DebugSystem.conf->ReadInt( "Main", "logsize" );
    m_logTimeout     = g_DebugSystem.conf->ReadInt( "Main", "logtimeout" );
    m_logDirectory   = g_DebugSystem.fullpath( g_DebugSystem.conf->ReadString( "Main", "logdir" ) );
    m_logFileHandle  = NULL;
    m_udpRemoteHost  = g_DebugSystem.conf->ReadString( "Main", "logsendudp" );
    m_udpSocket      = 0;

    m_isProgressVisible = false;
    m_progressLabel     = new TUILabel( this, "ProgressLabel", 0, 0, 0, 0, UIBLUE, "", UIL );
    m_progressLabel->visible = false;
}

TLogPanel::~TLogPanel( )
{
    FUNCTION_TRACE
    m_logLines.clear();
    xfclose( m_logFileHandle );
    if( m_udpSocket != 0 ) close( m_udpSocket );
}

//-------------------------------------------------------------------------------------------

void TLogPanel::Initialize( )
{
    FUNCTION_TRACE
    if( !direxist( m_logDirectory ) )
    {
        AddLogEntry( CLR(f_RED) "logdir : " + m_logDirectory + " not exists, using ./log" CLR0 "\n" );
        m_logDirectory = g_DebugSystem.fullpath( "./log" );
    }

    AddLogEntry( "logdir : " CLR(f_GREEN) + m_logDirectory + CLR0 "\n" );

    string logName = nowrdate('_') + "_" + nowtime('_') + ".log";

    // Подбор уникального имени файла (с суффиксом, если файл уже есть)
    while( true )
    {
        m_fullLogPath = m_logDirectory + "/" + logName;
        if( !fileexist( m_fullLogPath ) ) break;

        size_t dotPos = logName.find_last_of('.');
        string baseName = (dotPos != string::npos) ? logName.substr(0, dotPos) : logName;

        int suffix = 1;
        size_t openBracket = baseName.find_last_of('(');
        size_t closeBracket = baseName.find_last_of(')');

        if( closeBracket != string::npos && openBracket != string::npos && openBracket < closeBracket )
        {
            suffix = str2int( baseName.substr(openBracket + 1, closeBracket - openBracket - 1) ) + 1;
            baseName = baseName.substr(0, openBracket - 1);
        }
        logName = stringformat("%s (%d).log", baseName.c_str(), suffix);
    }

    m_logFileHandle = fopen( m_fullLogPath.c_str(), "w" );
}

//-------------------------------------------------------------------------------------------

void TLogPanel::resize( )
{
    FUNCTION_TRACE
    doresize( MenuPanel->w, 1, COLS - MenuPanel->w - SensorPanel->w, LINES - 2 );

    m_lastLineIt = m_logLines.end();
    for( int i = 0; (i < h) && (m_lastLineIt != m_logLines.begin()); i++, m_lastLineIt-- );

    m_scrollLineIt = m_lastLineIt;
    m_scrollPosition = (m_currentLogSize > h) ? m_currentLogSize - h : 0;
}

//-------------------------------------------------------------------------------------------

void TLogPanel::draw( )
{
    FUNCTION_TRACE
    m_progressLabel->visible = false;
    char lineBuffer[1024];

    LogLinesIt lineIt = m_scrollLineIt;
    for( int row = 0; (row < h) && (lineIt != m_logLines.end()); row++, lineIt++ )
    {
        strncpy( lineBuffer, lineIt->c_str(), sizeof(lineBuffer) - 1 );
        size_t len = strlen(lineBuffer);
        if( len > 0 && lineBuffer[len - 1] == '\n' ) lineBuffer[len - 1] = 0;

        char* currentPos = lineBuffer;
        int cursorX = 1;
        int activeColor, nextColor;

        setcolor( window, color );
        mvwprintw( window, row, cursorX, "%*s", w - cursorX - 1, "" );

        while( true )
        {
            char* ansiEscape = strstr( currentPos, "\033[" );
            if( ansiEscape ) *ansiEscape = 0;

            string textSegment = string(currentPos);
            int symbolsToPrint = utf8indexsymbols( textSegment, w - cursorX - 1 );
            mvwprintw( window, row, cursorX, "%s", textSegment.substr(0, symbolsToPrint).c_str() );
            cursorX += utf8length(textSegment);

            if( !ansiEscape ) break;

            char* colorEnd = strchr( ansiEscape + 1, 'm' );
            if( !colorEnd ) break;

            if( colorEnd - ansiEscape - 3 > 0 )
            {
                activeColor = nextColor = 0;
                sscanf( ansiEscape + 2, "%d;%d", &activeColor, &nextColor );
                if( nextColor ) activeColor = nextColor;
            }
            else activeColor = color;

            setcolor( window, activeColor );
            currentPos = colorEnd + 1;
        }
        // Отрисовка прогресс-бара в конце последней строки
        if( *lineIt == m_logLines.back() )
        {
            setcolor( window, UIBLUE );
            mvwprintw( window, row, cursorX, " " );
            if( m_isProgressVisible )
            {
                m_progressLabel->resize( cursorX, row, w - cursorX - 1, 1 );
                m_progressLabel->visible = true;
            }
        }
    }

    // Рамки и скроллбар
    setcolor( window, focused ? -UIGREEN : color );
    mvwvline( window, 0, 0, ACS_VLINE, h );
    mvwvline( window, 0, w - 1, ACS_VLINE, h );

    if( m_currentLogSize > h )
    {
        setcolor( window, focused ? +UIGREEN : -color );
        double ratio = double(h) / m_currentLogSize;
        int barHeight = int(Round( h * ratio )); if( barHeight == 0) barHeight = 1;
        int barY = int(Round( m_scrollPosition * ratio ));
        if( barY >= h ) barY = h - barHeight;
        mvwvline( window, barY, w - 1, ' ', barHeight );
    }

    drawobjects( );
}

//-------------------------------------------------------------------------------------------

bool TLogPanel::event( int inputKey, char* /*codes*/ )
{
    FUNCTION_TRACE
    if( ESCAPE(inputKey) ) MenuPanel->HandleEscape( );

    LogLinesIt previousScroll = m_scrollLineIt;
    switch( inputKey )
    {
        case KEY_UP:
            if( m_scrollLineIt == m_logLines.begin() ) break;
            m_scrollLineIt--;
            m_scrollPosition--;
            RefreshView( );
            break;

        case KEY_DOWN:
            if( m_scrollLineIt == m_lastLineIt ) break;
            m_scrollLineIt++;
            m_scrollPosition++;
            RefreshView( );
            break;

        case KEY_PPAGE: // PageUp
            for( int i = 0; (i < h) && (m_scrollLineIt != m_logLines.begin()); i++, m_scrollLineIt--, m_scrollPosition-- );
            if( previousScroll != m_scrollLineIt ) RefreshView( );
            break;

        case KEY_NPAGE: // PageDown
            for( int i = 0; (i < h) && (m_scrollLineIt != m_lastLineIt); i++, m_scrollLineIt++, m_scrollPosition++ );
            if( previousScroll != m_scrollLineIt ) RefreshView( );
            break;
    }
    return true;
}

//-------------------------------------------------------------------------------------------

void TLogPanel::ScrollPageUp( )   { event( KEY_PPAGE, NULL ); }
void TLogPanel::ScrollPageDown( ) { event( KEY_NPAGE, NULL ); }

void TLogPanel::RefreshView( )
{
    FUNCTION_TRACE
    draw();
    uirefresh( );
}

void TLogPanel::RefreshProgress( )
{
    FUNCTION_TRACE
    m_progressLabel->draw();
    uirefresh( );
}

//-------------------------------------------------------------------------------------------

void TLogPanel::AddLogEntry( const std::string& text )
{
    FUNCTION_TRACE
    DLOCK( m_logMutex );

    if( text == "#PROGRESS-ON#" )
    {
        m_isProgressVisible = true;
        m_progressLabel->text = "";
        UI->DoLock( LOG_UPDATE );
    }
    else if( text == "#PROGRESS-OFF#" )
    {
        m_isProgressVisible = false;
    }
    else if( text.compare(0, 4, "<UI>") == 0 )
    {
        m_progressLabel->text = rtrim( text.substr(4) );
        UI->DoLock( PROGRESS_UPDATE );
    }
    else
    {
        size_t start = 0;
        while( true )
        {
            size_t end = text.find( '\n', start );
            if( end != std::string::npos ) {
                internalAddLine( text.substr(start, end - start + 1) );
                start = end + 1;
            }
            else {
                internalAddLine( text.substr(start) );
                break;
            }
        }

        string cleanText = removecolor( text );
        if( m_logFileHandle )
        {
            fprintf( m_logFileHandle, "%s", cleanText.c_str() );
            fflush( m_logFileHandle );
            fsync( fileno(m_logFileHandle) );
        }
        UI->DoLock( LOG_UPDATE );
    }
}

//-------------------------------------------------------------------------------------------

std::string TLogPanel::FinalizeAndCopyLog( const bool& isTestPassed, const std::string& serialNumber )
{
    if( m_logFileHandle )
    {
        string baseFileName = getFilenameFromPath(m_fullLogPath);
        size_t extensionPos = baseFileName.find(".log");

        string newFileName = baseFileName.substr(0, extensionPos);
        newFileName += "_" + serialNumber;
        newFileName += isTestPassed ? ".ok" : ".fail";

        string fullDestinationPath = m_logDirectory + "/" + newFileName;

        if( copyFile( m_fullLogPath, fullDestinationPath ) )
            return fullDestinationPath;
    }
    return "";
}

//-------------------------------------------------------------------------------------------

void TLogPanel::internalAddLine( const std::string& lineText )
{
    FUNCTION_TRACE
    // Если последняя строка не закончена переносом - дополняем её
    if( !m_logLines.empty() && m_logLines.back().find('\n') == std::string::npos )
    {
        m_logLines.back().append( lineText );
    }
    else
    {
        m_logLines.push_back( lineText );
        if( ++m_currentLogSize > m_maxLogSize ) {
            m_logLines.pop_front( );
            m_currentLogSize--;
        }

        if( m_currentLogSize > h ) m_lastLineIt++;

        m_scrollLineIt = m_lastLineIt;
        m_scrollPosition = (m_currentLogSize > h) ? m_currentLogSize - h : 0;
    }
}

//-------------------------------------------------------------------------------------------

void toLog( const char* format, ... )
{
    char buffer[4096];
    FormatToStr( format, buffer, sizeof(buffer) );
    toLog( string(buffer) );
}

void toLog( const std::string& text )
{
    FUNCTION_TRACE
    if( LogPanel ) LogPanel->AddLogEntry( text );
}
