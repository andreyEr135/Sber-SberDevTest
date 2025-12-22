#ifndef uitestbtnH
#define uitestbtnH

#include "ui.h"
#include <string>

//-------------------------------------------------------------------------------------------

/**
 * @brief Специализированная кнопка для запуска тестов.
 * Отличается от стандартной TUIButton наличием дополнительных статусных индикаторов при отрисовке.
 */
class TUITestButton : public TUIButton
{
  protected:
    virtual bool event( int inputKey, char* scanCodes );

  public:
    TUITestButton( const TUIPanel* parentPanel,
                   const std::string& name,
                   int x, int y, int w, int h,
                   int colorNormal,
                   int colorFocused,
                   const std::string& label,
                   int alignment,
                   void (*OnPress)( TUIButton* btn ) );

    virtual ~TUITestButton( );

    virtual void draw( );
};

//-------------------------------------------------------------------------------------------
#endif
