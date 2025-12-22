#ifndef mainH
#define mainH
//-------------------------------------------------------------------------------------------

#include "debugsystem.h"

#include "ui.h"

#define UIBLACK    1
#define UIBLUE     2
#define UIGREEN    3
#define UIRED      4
#define UIYELLOW   5
#define UIblack   30
#define UIred     31
#define UIgreen   32
#define UIyellow  33
#define UIblue    34
#define UImagenta 35
#define UIcyan    36
#define UIwhite   37

#define SENSOR_UPDATE       ( UIEVENT_USER +  1 )
#define TIMER_AUTOSTART     ( UIEVENT_USER +  2 )
#define LOG_UPDATE          ( UIEVENT_USER +  3 )
#define PROGRESS_UPDATE     ( UIEVENT_USER +  4 )
#define TEST_UPDATE         ( UIEVENT_USER +  5 )
#define DIALOG_SHOW         ( UIEVENT_USER +  6 )
#define DIALOG_HIDE         ( UIEVENT_USER +  7 )
#define TESTCONTROL_UPDATE  ( UIEVENT_USER +  8 )
#define RESCANINI           ( UIEVENT_USER +  9 )
#define COMPLEXSTART        ( UIEVENT_USER + 10 )
#define COMPLEXSTOP         ( UIEVENT_USER + 11 )


#define MAIN_UNIT           ( UNIT_NEXT +  0 )
#define UI_UNIT             ( UNIT_NEXT +  1 )
#define UIPANEL_UNIT        ( UNIT_NEXT +  2 )
#define UIOBJECT_UNIT       ( UNIT_NEXT +  3 )
#define UILABEL_UNIT        ( UNIT_NEXT +  4 )
#define UIBUTTON_UNIT       ( UNIT_NEXT +  5 )
#define UIEDIT_UNIT         ( UNIT_NEXT +  6 )
#define UIMEMO_UNIT         ( UNIT_NEXT +  7 )
#define TEST_UNIT           ( UNIT_NEXT +  8 )
#define AUTOSTARTPANEL_UNIT ( UNIT_NEXT +  9 )
#define BACKPANEL_UNIT      ( UNIT_NEXT + 10 )
#define DIALOGPANEL_UNIT    ( UNIT_NEXT + 11 )
#define HELLOPANEL_UNIT     ( UNIT_NEXT + 12 )
#define LOGPANEL_UNIT       ( UNIT_NEXT + 13 )
#define MENUPANEL_UNIT      ( UNIT_NEXT + 14 )
#define SENSORPANEL_UNIT    ( UNIT_NEXT + 15 )
#define EDITPANEL_UNIT      ( UNIT_NEXT + 16 )
#define UITESTBTN_UNIT      ( UNIT_NEXT + 17 )
#define TCP_UNIT            ( UNIT_NEXT + 18 )
#define PROGRAMPANEL_UNIT   ( UNIT_NEXT + 19 )
#define FWMAC_UNIT          ( UNIT_NEXT + 20 )

extern TVERSION   VERSION;

void OnPressOkDialogBtn( TUIButton* btn );

void RunEventScript( std::string event );

//-------------------------------------------------------------------------------------------
#endif

