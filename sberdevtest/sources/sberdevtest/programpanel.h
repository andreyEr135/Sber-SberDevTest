#ifndef firmwarepanelH
#define firmwarepanelH

#include "ui.h"
#include "uimemo.h"

//-------------------------------------------------------------------------------------------

class FirmwareData;

/**
 * @brief Панель ввода данных для прошивки устройства
 */
class TDeviceFirmwarePanel : public TUIPanel
{
  protected:
    TUIButton* m_setBtn;

    virtual void resize( );
    virtual void draw( );
    virtual bool event( int inputKey, char* scanCodes );

  public:
    TDeviceFirmwarePanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    virtual ~TDeviceFirmwarePanel( );

    TUIEdit* m_serialEdit;
    TUIEdit* m_uuidEdit;
    TUIEdit* m_macEdit;

    FirmwareData* m_firmwareLogic;
};

//-------------------------------------------------------------------------------------------

/**
 * @brief Контейнер для управления процессом прошивки
 */
class TProgramPanel
{
  public:
    TProgramPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    ~TProgramPanel( );

    TDeviceFirmwarePanel* m_inputPanel;
    int m_panelWidth;

    void ShowPanel( );
    void HidePanel( );
};

//-------------------------------------------------------------------------------------------

extern TProgramPanel* ProgramPanel;

#endif
