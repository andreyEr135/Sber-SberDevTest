#ifndef editpanelH
#define editpanelH

#include "ui.h"
#include "uimemo.h"
#include <string>

//-------------------------------------------------------------------------------------------

class TEditPanel : public TUIPanel
{
  protected:
    virtual void resize( );
    virtual void draw( );
    virtual bool event( int inputKey, char* scanCodes );

    TUIMemo*    m_memo;
    TUILabel*   m_topLabel;
    TUILabel*   m_bottomLabel;

    // Коллбэк при закрытии редактора
    void (*m_onExitCallback)( std::string fileName, bool wasChanged );

    std::string m_directoryPath;
    std::string m_fileName;
    bool        m_fileWasModified;

  public:
    TEditPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color );
    virtual ~TEditPanel( );

    void ShowFile( const std::string fullPath, void (*onExit)( std::string file, bool changed ) = NULL );
    void ClosePanel( );

    bool SaveFile( );
    bool SaveFileAs( std::string newFileName );
};

//-------------------------------------------------------------------------------------------

extern TEditPanel* EditPanel;

#endif
