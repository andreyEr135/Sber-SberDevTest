#ifndef dialogpanelH
#define dialogpanelH

#include <string>
#include <list>
#include <iterator>
#include "ui.h"

//-------------------------------------------------------------------------------------------

class TDialogPanel : public TUIPanel
{
  protected:
    enum DialogMode { MODE_MESSAGE, MODE_INPUT } m_currentMode;

    typedef std::list< TUIButton* >           ButtonList;
    typedef std::list< TUIButton* >::iterator ButtonListIt;

    virtual void resize( );
    virtual void draw( );
    virtual bool event( int inputKey, char* scanCodes );

    std::string    m_windowTitle;
    std::string    m_messageText;
    ButtonList     m_activeButtons;
    TUIEdit*       m_inputField;

    std::string    m_setupString; // Строка параметров вида "W,H{Title}{Text}{Buttons}{Default}"
    void (*m_onButtonClickCallback)( TUIButton* btn );
    bool           m_shouldCloseMenuOnEscape;

  public:
    TDialogPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color );
    virtual ~TDialogPanel( );

    // Синхронные методы (вызываются из потока UI)
    void SyncShow( );
    void SyncHide( );

    // Публичный интерфейс (потокобезопасный)
    void ShowMessage( int windowColor, const std::string config, void (*onBtnClick)( TUIButton* btn ), bool escapeToMenu = false );
    void ShowInput( int windowColor, const std::string config, void (*onBtnClick)( TUIButton* btn ), bool escapeToMenu = false );
    void Close( );

    std::string GetInputText( ) { return m_inputField ? m_inputField->text : ""; }
};

//-------------------------------------------------------------------------------------------

extern TDialogPanel* DialogPanel;

#endif
