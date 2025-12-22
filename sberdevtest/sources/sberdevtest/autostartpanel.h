#ifndef autostartpanelH
#define autostartpanelH

#include <time.h>
#include <sys/time.h>
#include "ui.h"

//-------------------------------------------------------------------------------------------

class TAutoStartPanel : public TUIPanel
{
  private:
    TUILabel* m_iniFileLabel;
    TUILabel* m_commentLabel;
    TUILabel* m_countDownLabel;
    timer_t   m_autoStartTimer;
    int       m_remainingSeconds;

  protected:
    virtual void draw( );
    virtual void resize( );
    virtual bool event( int inputKey, char* scanCodes );

  public:
    TAutoStartPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    virtual ~TAutoStartPanel( );

    void ShowPanel( );
    void UpdateTick( );
    void ResetCountdown( );
};

//-------------------------------------------------------------------------------------------

extern TAutoStartPanel* AutoStartPanel;

#endif
