#ifndef menupanelH
#define menupanelH

#include "ui.h"
#include <string>

//-------------------------------------------------------------------------------------------

class TMenuPanel : public TUIPanel
{
  protected:
    virtual void resize( );
    virtual void draw( );
    virtual bool event( int inputKey, char* scanCodes );

    enum MenuViewState {
        VIEW_MAIN,
        VIEW_COMPLEX,
        VIEW_CUSTOM,
        VIEW_RUN_COMPLEX,
        VIEW_RUN_CUSTOM,
        VIEW_RUN_SINGLE
    } m_currentView;

    void hideAllObjects( );

  public:
    TMenuPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color );
    virtual ~TMenuPanel( );

    // Кнопки основного меню
    TUIButton* m_complexBtn;
    TUIButton* m_customBtn;
    TUIButton* m_scannerBtn;
    TUIButton* m_settingsBtn;
    TUIButton* m_backBtn;

    // Элементы режима выполнения
    TUILabel*  m_cyclesLabel;
    TUILabel*  m_runStatusLabel;
    TUIButton* m_stopBtn;

    // Элементы комплексного теста
    TUILabel*  m_complexHeader;
    TUIEdit*   m_complexCyclesEdit;
    TUIButton* m_complexStartBtn;

    // Элементы выборочного теста
    TUILabel*  m_customHeader;
    TUIEdit*   m_customCyclesEdit;

    // Вспомогательный указатель на последнюю выбранную кнопку теста
    TUIButton* m_lastFocusedCustomBtn;

    void ShowMainMenu( );
    void ShowComplexMenu( );
    void ShowCustomMenu( );
    void ShowRunningState( );

    void HandleEscape( );
    void UpdateState( const std::string& command );
};

//-------------------------------------------------------------------------------------------

extern TMenuPanel* MenuPanel;

// Коллбэки
void OnMenuButtonClick( TUIButton* btn );
void OnTestButtonClick( TUIButton* btn );

#endif
