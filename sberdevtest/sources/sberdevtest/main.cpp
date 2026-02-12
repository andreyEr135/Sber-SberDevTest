#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <ncurses.h>
#include <locale.h>

#include <string>
#include <list>
#include <iterator>
#include <algorithm>

#include "debugsystem.h"

#include "backpanel.h"
#include "dialogpanel.h"
#include "autostartpanel.h"
#include "hellopanel.h"
#include "menupanel.h"
#include "logpanel.h"
#include "sensorpanel.h"
#include "editpanel.h"
#include "test.h"
#include "programpanel.h"
#include "tcpclient.h"

#include "main.h"

#include "_version"

using namespace std;

//------------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ MAIN_UNIT ]
#include "log_macros.def"

pid_t script_pid = 0;

//------------------------------------------------------------------------------
void RunEventScript( std::string event )
{FUNCTION_TRACE
    string script = g_DebugSystem.conf->ReadString( "Event", event );
	if( script == "" ) return;
	script = g_DebugSystem.fullpath( script );
	Log( "Event %s: script '%s'\n", event.c_str(), script.c_str() );
	//if( !fileexist( script ) ) { Warning( "script '%s' not found\n", script.c_str() ); return; }
	//if( system( script.c_str() ) != 0 ) Error( "script '%s'\n", script.c_str() );

	FILE *instream, *outstream;
	script_pid = RunProcessIO( script.c_str(), instream, outstream );
	while( instream && !feof( instream ) )
	{
		int c = fgetc( instream );
		if( c == EOF )  break;
		printf( "%c", c );
	}
	int status = WaitProcessIO( script_pid, instream, outstream );
	if( status != 0 ) Error( "script '%s' return[ %d ]\n", script.c_str(), status );
	script_pid = 0;
}
//------------------------------------------------------------------------------

void OnPressExitDialogBtn( TUIButton* btn )
{FUNCTION_TRACE
    DialogPanel->Close( );
	string answer = trim(btn->text);
	if( answer == "Да" ) UI->exit( );
}
//-------------------------------------------------------------------------------------------

void OnPressHaltDialogBtn( TUIButton* btn )
{FUNCTION_TRACE
    DialogPanel->Close( );
	string answer = trim(btn->text);
	if( answer == "Выключить" )
	{
		RunEventScript( "halt" );
		UI->exit( );
		if( system( "/sbin/poweroff" ) != 0 ) Error( "system( /sbin/poweroff )\n" );
	}
	if( answer == "Перезагрузить" )
	{
		UI->exit( );
		RunEventScript( "reboot" );
		if( system( "/sbin/reboot" ) != 0 ) Error( "system( /sbin/reboot )\n" );
	}
}
//-------------------------------------------------------------------------------------------

void OnPressOkDialogBtn( TUIButton* btn )
{FUNCTION_TRACE
    DialogPanel->Close( );
}
//-------------------------------------------------------------------------------------------

bool OnUIEvent( TUI* ui, int event )
{FUNCTION_TRACE
	switch( event )
	{
		case UIEVENT_INIT:
			if( !has_colors() ) break;
			if( start_color( ) == ERR ) Error( "start_color\n" );
			init_pair( UIBLACK,   COLOR_WHITE,   COLOR_BLACK );
			init_pair( UIBLUE,    COLOR_WHITE,   COLOR_BLUE );
			init_pair( UIGREEN,   COLOR_BLACK,   COLOR_GREEN );
			init_pair( UIRED,     COLOR_BLACK,   COLOR_RED );
			init_pair( UIYELLOW,  COLOR_BLACK,   COLOR_YELLOW );
			init_pair( UIblack,   COLOR_BLACK,   COLOR_WHITE ); 
			init_pair( UIred,     COLOR_RED,     COLOR_BLACK );
			init_pair( UIgreen,   COLOR_GREEN,   COLOR_BLACK );
			init_pair( UIyellow,  COLOR_YELLOW,  COLOR_BLACK );
			init_pair( UIblue,    COLOR_BLUE,    COLOR_BLACK );
			init_pair( UImagenta, COLOR_MAGENTA, COLOR_BLACK );
			init_pair( UIcyan,    COLOR_CYAN,    COLOR_BLACK );
			init_pair( UIwhite,   COLOR_WHITE,   COLOR_BLACK );
			break;
		
        case UIEVENT_EXIT       : DialogPanel->ShowMessage( UIBLUE, "30,9{}{Выйти из программы?}{Да|Нет}{Нет}", &OnPressExitDialogBtn ); break;

        case SENSOR_UPDATE      : SensorPanel->Refresh( ); break;
        case TIMER_AUTOSTART    : AutoStartPanel->UpdateTick( ); break;
        case LOG_UPDATE         : LogPanel->RefreshView( ); break;
        case PROGRESS_UPDATE    : LogPanel->RefreshProgress( ); break;
		case TEST_UPDATE        : Test->Update( ); break;
        case DIALOG_SHOW        : DialogPanel->SyncShow( ); break;
        case DIALOG_HIDE        : DialogPanel->SyncHide( ); break;
        case RESCANINI          : HelloPanel->ShowPanel( ); break;
        case COMPLEXSTART       : BackPanel->StartComplexTest( ); break;
        case COMPLEXSTOP        : BackPanel->StopComplexTest( ); break;
	}
	return true;
}
//-------------------------------------------------------------------------------------------

void Hotkey( int key, char* codes )
{FUNCTION_TRACE
    if( key == KEY_F(10) ) DialogPanel->ShowMessage( UIRED, "54,9{}{Выключение}{Выключить|Перезагрузить|Отмена}{Отмена}", &OnPressHaltDialogBtn );
}
//-------------------------------------------------------------------------------------------

bool OnStopSignal( int signum )
{FUNCTION_TRACE
	Log( "main: OnStopSignal\n" );
	if( script_pid != 0 )
	{
		g_DebugSystem.kill_process_and_childs( script_pid );
		return false;
	}
	else
	{
		curs_set( 1 );
		endwin( );
		RunEventScript( "exit" );
	}
//	if( control ) control->sendlog( "stopsignal %d", signum );
//	xdelete( control );
	return true;
}
//-------------------------------------------------------------------------------------------

int main( int argc, char **argv )
{
	g_DebugSystem.addunit( "[main]"     , MAIN_UNIT           );
	g_DebugSystem.addunit( "[ui]"       , UI_UNIT             );
	g_DebugSystem.addunit( "[uipanel]"  , UIPANEL_UNIT        );
	g_DebugSystem.addunit( "[uiobject]" , UIOBJECT_UNIT       );
	g_DebugSystem.addunit( "[uilabel]"  , UILABEL_UNIT        );
	g_DebugSystem.addunit( "[uibutton]" , UIBUTTON_UNIT       );
	g_DebugSystem.addunit( "[uiedit]"   , UIEDIT_UNIT         );
	g_DebugSystem.addunit( "[uimemo]"   , UIMEMO_UNIT         );
	g_DebugSystem.addunit( "[test]"     , TEST_UNIT           );
	g_DebugSystem.addunit( "[autostart]", AUTOSTARTPANEL_UNIT );
	g_DebugSystem.addunit( "[back]"     , BACKPANEL_UNIT      );
	g_DebugSystem.addunit( "[dialog]"   , DIALOGPANEL_UNIT    );
	g_DebugSystem.addunit( "[hello]"    , HELLOPANEL_UNIT     );
	g_DebugSystem.addunit( "[log]"      , LOGPANEL_UNIT       );
	g_DebugSystem.addunit( "[menu]"     , MENUPANEL_UNIT      );
	g_DebugSystem.addunit( "[sensor]"   , SENSORPANEL_UNIT    );
	g_DebugSystem.addunit( "[edit]"     , EDITPANEL_UNIT      );
	g_DebugSystem.addunit( "[uitestbtn]", UITESTBTN_UNIT      );
    g_DebugSystem.addunit( "[tcpclient]", TCP_UNIT            );
	g_DebugSystem.init( argc, argv, &VERSION, true, LOGTIME_UPTIME );
	g_DebugSystem.OnStopSignal = &OnStopSignal;
	FUNCTION_TRACE

	RunEventScript( "start" );

	if( !getenv( "TERM" ) ) { Error( "env TERM not found\n" ); return -1; }

	KEYTRACE = g_DebugSystem.checkparam( "key" );
	
	int uidelay = g_DebugSystem.conf->ReadInt( "Main", "uidelay" );
	if( uidelay > 0 ) Usleep( uidelay*1000 );

	UI = new TUI( &OnUIEvent, &Hotkey );
	
	BackPanel      = new TBackPanel     ( UI, "backpanel",      0, 0,  0,  0, UIBLACK );
	DialogPanel    = new TDialogPanel   ( UI, "dialogpanel",    0, 0,  0,  0, UIBLUE  );
	AutoStartPanel = new TAutoStartPanel( UI, "autostartpanel", 0, 0, 40,  8, UIBLUE  );
	HelloPanel     = new THelloPanel    ( UI, "hellopanel",     0, 0, 80, 22, UIBLACK );
	MenuPanel      = new TMenuPanel     ( UI, "menupanel",      0, 0, 21,  0, UIBLACK );
    LogPanel       = new TLogPanel      ( UI, "logpanel",       0, 0,  0,  0, UIBLACK ); LogPanel->Initialize();
	SensorPanel    = new TSensorPanel   ( UI, "sensorpanel",    0, 0, 16,  0, UIBLACK );
	EditPanel      = new TEditPanel     ( UI, "editpanel",      0, 0,  0,  0, UIBLACK );
    ProgramPanel   = new TProgramPanel  ( UI, "programpanel",   0, 0, 80, 28, UIBLACK );

    Tcp = new TcpClient();
	Test = new TTest( );


	RunEventScript( "show" );

    BackPanel->show( );
    HelloPanel->m_selectionPanel->IdentifyHardware();

    std::string serial = HelloPanel->m_selectionPanel->m_serialNumber;
    std::string uuid = HelloPanel->m_selectionPanel->m_uuid;


    /*if ((HelloPanel->m_selectionPanel->m_serialNumber.empty()) || (HelloPanel->m_selectionPanel->m_uuid.empty())) ProgramPanel->ShowPanel();
    else {
        HelloPanel->ShowPanel( );
        std::string sendMsg = stringformat("#sn#%s#", HelloPanel->m_selectionPanel->m_serialNumber.c_str());
        Tcp->sendData(sendMsg.c_str(), sendMsg.length());
    }*/
    // Проверяем: пустота ИЛИ "Default string" в серийнике ИЛИ пустота в UUID
    if (serial.empty() || serial == "Default string" || uuid.empty()) {
        ProgramPanel->ShowPanel();
    }
    else {
        HelloPanel->ShowPanel();
        std::string sendMsg = stringformat("#sn#%s#", serial.c_str());
        Tcp->sendData(sendMsg.c_str(), sendMsg.length());
    }

	UI->resize( );
	UI->draw( );

	UI->Run( );
	
	xdelete( Test );
	xdelete( UI );
	g_DebugSystem.logsetstdout( true );

	RunEventScript( "exit" );

	return 0;
}
//-------------------------------------------------------------------------------------------

