#ifndef backpanelH
#define backpanelH

#include "ui.h"

//-------------------------------------------------------------------------------------------

class TBackPanel : public TUIPanel
{
  private:
    // Здесь только внутренние переменные логики, если появятся

  protected:
    virtual void resize( );
    virtual void draw( );

  public:
    TBackPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    virtual ~TBackPanel( );

    // Оставляем ПУБЛИЧНЫМИ, чтобы другие панели могли писать: BackPanel->StatusLabel->text = "..."
    TUILabel* TitleLabel;
    TUILabel* DeviceLabel;
    TUILabel* StatusLabel;
    TUILabel* TestControlLabel;

    void ShowMainLayout( );
    void HideMainLayout( );

    void ShowProgramLayout( );
    void HideProgramLayout( );

    void StartComplexTest( );
    void StopComplexTest( );
};

//-------------------------------------------------------------------------------------------

extern TBackPanel* BackPanel;

#endif
