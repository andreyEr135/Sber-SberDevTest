#ifndef hellopanelH
#define hellopanelH

#include "ui.h"
#include "uimemo.h"
#include <string>

//-------------------------------------------------------------------------------------------

/**
 * @brief Панель выбора доступных тестов (INI файлов)
 */
class TTestSelectionPanel : public TUIPanel
{
  protected:
    TUIButton* m_functionalTestBtn;
    TUIButton* m_stressTestBtn;

    virtual void resize( );
    virtual void draw( );
    virtual bool event( int inputKey, char* scanCodes );

  public:
    TTestSelectionPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    virtual ~TTestSelectionPanel( );

    std::string m_serialNumber;
    std::string m_uuid;

    void IdentifyHardware( );
    void RefreshAvailableTests( );
};

//-------------------------------------------------------------------------------------------

/**
 * @brief Главная приветственная панель (контейнер для выбора тестов)
 */
class THelloPanel
{
  public:
    THelloPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    ~THelloPanel( );

    TTestSelectionPanel* m_selectionPanel;
    int m_panelWidth;

    void ShowPanel( );
    void HidePanel( );
};

//-------------------------------------------------------------------------------------------

extern THelloPanel* HelloPanel;

#endif
