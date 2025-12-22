#ifndef logpanelH
#define logpanelH

#include <list>
#include <string>
#include <iterator>
#include <pthread.h>
#include "netservice.h"
#include "main.h"
#include "ui.h"

//-------------------------------------------------------------------------------------------

class TLogPanel : public TUIPanel
{
  private:
    std::string m_fullLogPath;

  protected:
    typedef std::list< std::string > LogLinesList;
    typedef LogLinesList::iterator   LogLinesIt;

    std::string  m_logDirectory;
    FILE*        m_logFileHandle;

    LogLinesList m_logLines;
    int          m_currentLogSize;
    int          m_maxLogSize;
    int          m_logTimeout;

    // Параметры для отправки логов по UDP
    std::string  m_udpRemoteHost;
    int          m_udpSocket;
    sockaddr_in  m_udpRemoteAddr;

    LogLinesIt   m_lastLineIt;    // Итератор последней строки для отрисовки
    LogLinesIt   m_scrollLineIt;  // Текущая позиция прокрутки
    int          m_scrollPosition;

    bool         m_isProgressVisible;
    TUILabel*    m_progressLabel;

    virtual void resize( );
    virtual void draw( );
    virtual bool event( int inputKey, char* scanCodes );

    Tmutex m_logMutex;
    void internalAddLine( const std::string& lineText );

  public:
    TLogPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    virtual ~TLogPanel( );

    void Initialize( );

    void AddLogEntry( const std::string& text );
    std::string FinalizeAndCopyLog( const bool& isTestPassed, const std::string& serialNumber );

    void RefreshView( );
    void RefreshProgress( );

    void ScrollPageUp( );
    void ScrollPageDown( );
};

// Глобальные функции для логирования
void toLog( const char* format, ... );
void toLog( const std::string& text );

extern TLogPanel* LogPanel;

//-------------------------------------------------------------------------------------------
#endif
