#include "debugsystem.h"

#include "main.h"
#include "ui.h"

using namespace std;

//-------------------------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UI_UNIT ]
#include "log_macros.def"

TUI*  UI;
bool  KEYTRACE;

struct TUIkey { const char* name; int key; }
UIkeys[] =
{
    { "kf1",  KEY_F(1)  },
    { "kf2",  KEY_F(2)  },
    { "kf3",  KEY_F(3)  },
    { "kf4",  KEY_F(4)  },
    { "kf5",  KEY_F(5)  },
    { "kf6",  KEY_F(6)  },
    { "kf7",  KEY_F(7)  },
    { "kf8",  KEY_F(8)  },
    { "kf9",  KEY_F(9)  },
    { "kf10", KEY_F(10) },
    { "kbs",  KEY_BACKSPACE },
    { "kdch1",KEY_DC    },
    { "kcud1",KEY_DOWN  },
    { "kend", KEY_END   },
    { "kent", KEY_ENTER },
    { "khome",KEY_HOME  },
    { "kcub1",KEY_LEFT  },
    { "knp",  KEY_NPAGE },
    { "kpp",  KEY_PPAGE },
    { "kcuf1",KEY_RIGHT },
    { "kcuu1",KEY_UP    }
};

//-------------------------------------------------------------------------------------------

int curs_on ( ) { int prev = curs_set( 1 ); errno=0; return prev; }
int curs_off( ) { int prev = curs_set( 0 ); errno=0; return prev; }

//-------------------------------------------------------------------------------------------

void setcolor( WINDOW* window, int color )
{
    wbkgdset( window, (color>0) ? COLOR_PAIR(color) : COLOR_PAIR(-color)|A_REVERSE );
}
//-------------------------------------------------------------------------------------------

void drawlineH( WINDOW* window, int x, int y, int w )
{
    if( mvwaddch( window, y,x, ACS_LTEE ) == ERR ) Error( "mvwaddch\n" );
    if( mvwhline( window, y,x+1, ACS_HLINE, w-2 ) == ERR ) Error( "mvwhline\n" );
    if( mvwaddch( window, y,x+w-1, ACS_RTEE) == ERR ) Error( "mvwaddch\n" );
}
//-------------------------------------------------------------------------------------------

void drawlineV( WINDOW* window, int x, int y, int h )
{
    if( mvwaddch( window, y,x, ACS_TTEE ) == ERR ) Error( "mvwaddch\n" );
    if( mvwvline( window, y+1,x, ACS_VLINE, h-2 ) == ERR ) Error( "mvwvline\n" );
    if( mvwaddch( window, y+h-1,x, ACS_BTEE) == ERR ) Error( "mvwaddch\n" );
}
//-------------------------------------------------------------------------------------------

void uirefresh( )
{FUNCTION_TRACE
    update_panels( );
    if( doupdate( ) == ERR ) { Error( "doupdate\n" ); errno = 0; }
}
//-------------------------------------------------------------------------------------------



TUI::TUI( bool(*OnUIEvent)( TUI* ui, int event ), void(*Hotkey)( int key, char* codes ) ) : OnUIEvent(OnUIEvent),Hotkey(Hotkey)
{FUNCTION_TRACE
    listpanel.clear( );
    focuspanel = NULL;
    prevfocuslist.clear();
    fexit = false;
    ftablock = false;
    do_event = 0;

    strncpy( terminaldefault.type, getenv( "TERM" ), sizeof(terminaldefault.type)-1 );
    strncpy( terminaldefault.dev, ttyname( 0 ), sizeof(terminaldefault.dev)-1 );
    terminaldefault.pid = 0;
    terminalcurrent = terminaldefault;
    opencurses( );
}
//-------------------------------------------------------------------------------------------

TUI::~TUI( )
{FUNCTION_TRACE
    while( !listpanel.empty() ) delete listpanel.front();
    prevfocuslist.clear();
    closecurses( );
}
//-------------------------------------------------------------------------------------------

TUI::Tkeynode::Tkeynode( Tkeynode* parent, int code )
{FUNCTION_TRACE
    this->code = code;
    if( parent == NULL ) return;
    this->key = ERR;
    parent->childs.push_back( this );
    Debug( "%d <- %d\n", parent->code, code );
}
//-------------------------------------------------------------------------------------------

TUI::Tkeynode::~Tkeynode( )
{FUNCTION_TRACE
    for( TnodelistIt it = childs.begin(); it != childs.end(); ++it ) delete (*it);
    childs.clear( );
    Debug( "del %d\n", code );
}
//-------------------------------------------------------------------------------------------

TUI::Tkeynode* TUI::Tkeynode::findchild( int code )
{
    for( TnodelistIt it = childs.begin(); it != childs.end(); ++it )
        if( (*it)->code == code ) return (*it);
    return NULL;
}

//-------------------------------------------------------------------------------------------

bool TUI::opencurses( )
{FUNCTION_TRACE
    Log( "opencurses: %s %s\n", terminalcurrent.type, terminalcurrent.dev );

    terminalfp = fopen( terminalcurrent.dev, "rb+" );
    if( !terminalfp ) { Error( "fopen %s\n", terminalcurrent.dev ); return false; }

    terminalscreen = newterm( terminalcurrent.type, terminalfp, terminalfp );
    if( !terminalscreen ) { Error( "newterm %s\n", terminalcurrent.type ); return false; }

    set_term( terminalscreen );

    if( leaveok( stdscr, TRUE ) == ERR ) Error( "leaveok\n" );
    if( cbreak( ) == ERR ) Error( "cbreak\n" );  // ввод немедленно доступен программе, иначе - будет буферизован до получения целой строки
    if( noecho( ) == ERR ) Error( "noecho\n" );  // не выводить вводимые символы
    if( keypad( stdscr, TRUE ) == ERR ) Error( "keypad\n" ); // все клавиши
    ESCDELAY = 0;  // задержка после кода 27
    timeout( 0 ); // msec, timeout для getch 0
    errno = 0;
    if( OnUIEvent ) (*OnUIEvent)( this, UIEVENT_INIT );

    g_DebugSystem.logsetstdout( false );

    //--- init UIkeys
    keyroot = new Tkeynode( NULL,0 );
    for( uint i = 0; i < sizeof(UIkeys)/sizeof(TUIkey); i++ )
    {
        char* codes = tigetstr( (char*)UIkeys[i].name );
        if( !codes ||( codes == (char*)(-1) ) ) continue;
        Tkeynode* node = keyroot;
        for( uint c = 0; c < strlen(codes); c++ )
        {
            Tkeynode* found = node->findchild( codes[c] );
            node = ( found ) ? found : new Tkeynode( node, codes[c] );
        }
        node->key = UIkeys[i].key;
    }

    return true;
}
//-------------------------------------------------------------------------------------------

void TUI::closecurses( )
{FUNCTION_TRACE
    g_DebugSystem.logsetstdout( true );
    Log( "closecurses\n" );

    clear( );
    curs_on( );
    refresh( );
    endwin( );

    delscreen( terminalscreen );
    xfclose( terminalfp );
    errno = 0;

    delete keyroot;
    keyroot = NULL;
}
//-------------------------------------------------------------------------------------------

void TUI::newresources( )
{FUNCTION_TRACE
    for( TUIPanelListIt it = listpanel.begin(); it != listpanel.end(); ++it ) ((TUIPanel*)(*it))->newresources( );
    for( uint i = 0; i < panelstack.size(); i++ )
    {
        TUIPanel* uip = panelstack.at(i);
        if( uip->visible ) uip->show( ); else uip->hide( );
    }
    panelstack.clear( );
}
//-------------------------------------------------------------------------------------------

void TUI::freeresources( )
{FUNCTION_TRACE
    PANEL* p = NULL;
    while( ( p = panel_above( p ) ) != NULL )
    {
        TUIPanel* uip = NULL;
        for( TUIPanelListIt it = listpanel.begin(); it != listpanel.end(); ++it )
            if( ((TUIPanel*)(*it))->panel == p ) uip = (TUIPanel*)(*it);
        if( uip ) panelstack.push_back( uip );
    }
    for( TUIPanelListIt it = listpanel.begin(); it != listpanel.end(); ++it ) ((TUIPanel*)(*it))->freeresources( );
}
//-------------------------------------------------------------------------------------------

void TUI::DoLock( int event )
{FUNCTION_TRACE
    if( fexit ) return;
    if( pthread_equal( pthread_self( ), g_DebugSystem.mainthread ) )
    {
        DoEvent( event );
    }
    else
    {
        UILOCK;

        do_event = event;
        Debug( "wait ...\n" );
        event_done.wait();
        Debug( "wait done !!!\n" );
    }
}
//-------------------------------------------------------------------------------------------

void TUI::DoEvent( int event )
{FUNCTION_TRACE
    Debug( "  [ %d ]\n", event );
    int n = 0;
    char codes[ 8 ]; memset( codes, 0, sizeof(codes) );

    if( ( event >= UIEVENT_USER )&&( event <= UIEVENT_USER_MAX ) ) // USER events
    {
        if( OnUIEvent ) (*OnUIEvent)( this, event );
        return;
    }
    else if( event == UIEVENT_TERM_OPEN )
    {
        freeresources( );
        closecurses( );
        opencurses( );
        newresources( );
        resize( );
        draw( );
        return;
    }
    else if( event == UIEVENT_TERM_RESIZE )
    {
        resizeterm( terminallines, terminalcols );
        return;
    }
    else if( event == KEY_RESIZE || event == KEY_F(9) )
    {
        clear( );
        resize( );
        draw( );
        return;
    }
    else if( event == 8 ) // backspace
    {
        event = KEY_BACKSPACE;
    }
    else if( event == 9 ) // TAB
    {
        if( ftablock ) return;
        if( tabpanel( ) ) draw( );
        return;
    }
    else if( ENTER(event) ) ;
    else if( ESCAPE(event) ) ;

    else if( (event&0xFF80) == 0x00 )  // (1 байт)  0aaa aaaa
    {
        if( event < 32 ) return;
        codes[ n++ ] = event;
    }
    else if( (event&0xFFE0) == 0xC0 )  // (2 байта) 110x xxxx 10xx xxxx
    {
        codes[ n++ ] = event;
        codes[ n++ ] = getch( );
    }
    else if( (event&0xFFF0) == 0xE0 ) // (3 байта) 1110 xxxx 10xx xxxx 10xx xxxx
    {
        codes[ n++ ] = event;
        codes[ n++ ] = getch( );
        codes[ n++ ] = getch( );
    }
    //... (4 байта) 1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx

    if(KEYTRACE)Log( "key %d n=%d [%d %d %d %d]\n", event, n, codes[0],codes[1],codes[2],codes[3] );

    if( Hotkey ) (*Hotkey)( event, codes );

    if( focuspanel && !focuspanel->doevent( event, codes ) )
    {
        if( fexit ) return;
        if( memcmp( &terminalcurrent, &terminaldefault, sizeof(Tterminal) ) == 0 ) (*OnUIEvent)( this, UIEVENT_EXIT );
        else (*OnUIEvent)( this, UIEVENT_TERM_CLOSE );
    }
}
//-------------------------------------------------------------------------------------------

void TUI::Run( )
{FUNCTION_TRACE
    int   key;
    int   code;

    while( !fexit )
    {
        code = getch( );

        //Info( "%d\n", code );

        if( code != ERR )
        {
            key = code;

            Tkeynode* node = keyroot->findchild( code );
            while( node )
            {
                if( ( code = getch( ) ) == ERR ) { key = node->code; break; }
                if( ( node = node->findchild( code ) ) == NULL ) { key = ERR; break; }
                if( node->key != ERR ) { key = node->key; break; }
            }
            errno = 0;
            if( key == ERR ) { Warning( "bad combination codes\n" ); while( getch( ) != ERR ){} continue; }

            if( KEYTRACE ) Log( "key %d\n", key );

            DoEvent( key );
        }
        else
        {
            if( do_event != 0 )
            {
                DoEvent( do_event );
                do_event = 0;
                event_done.set();
            }
            else
                Usleep( 10000 );
        }
    }
    event_done.set();
}
//-------------------------------------------------------------------------------------------

void TUI::addpanel( TUIPanel* panel )
{FUNCTION_TRACE
    Debug( "%s\n", panel->name.c_str() );
    listpanel.push_back( panel );
}
//-------------------------------------------------------------------------------------------

void TUI::removepanel( TUIPanel* panel )
{FUNCTION_TRACE
    Debug( "%s\n", panel->name.c_str() );
    listpanel.remove( panel );
}
//-------------------------------------------------------------------------------------------

bool TUI::tabpanel( )
{FUNCTION_TRACE
    TUIPanelListIt oldit = find( listpanel.begin(), listpanel.end(), focuspanel );
    if ( listpanel.end() == oldit ) { Error( "TUI::tabpanel focuspanel not found\n" ); return false; }
    TUIPanelListIt newit = oldit;

    while( 1 )
    {
        if( ++newit == listpanel.end() ) newit = listpanel.begin();
        if( newit == oldit ) break;

        TUIPanel* p = (TUIPanel*)(*newit);
        //Debug( "tab %s\n", p->name.c_str() );
        if( p->visible && p->keyable ) { setfocus( p ); break; }
    }
    return ( newit != oldit );
}
//-------------------------------------------------------------------------------------------

void TUI::resize( )
{FUNCTION_TRACE
    for( TUIPanelListIt it = listpanel.begin(); it != listpanel.end(); ++it ) ((TUIPanel*)(*it))->resize( );
}
//-------------------------------------------------------------------------------------------

void TUI::draw( )
{FUNCTION_TRACE
    for( TUIPanelListIt it = listpanel.begin(); it != listpanel.end(); ++it )
    {
        TUIPanel* p = (TUIPanel*)(*it);
        if( p->visible ) p->draw( );
    }
    if( focuspanel && focuspanel->focusobject ) focuspanel->focusobject->cursor( );

    uirefresh( );
}
//-------------------------------------------------------------------------------------------

void TUI::setfocus( TUIPanel* panel )
{FUNCTION_TRACE
    if( panel == NULL ) return;

    if( focuspanel == panel ) { focuspanel->show( ); draw( ); return; }

    prevfocuslist.remove( panel );

    if( focuspanel )
    {
        focuspanel->focused = false;
        prevfocuslist.push_back( focuspanel );
    }
    focuspanel = panel;
    focuspanel->focused = true;
    focuspanel->show( );
    draw( );

}
//-------------------------------------------------------------------------------------------

void TUI::setfocusprev( )
{FUNCTION_TRACE
    if( prevfocuslist.empty( ) ) return;
    focuspanel->focused = false;
    focuspanel = prevfocuslist.back( );
    prevfocuslist.pop_back( );
    focuspanel->focused = true;
    focuspanel->show( );
    draw( );
}
//-------------------------------------------------------------------------------------------

void TUI::tablock( bool lock ) { ftablock = lock; }
void TUI::exit( ) { fexit = true; }

//-------------------------------------------------------------------------------------------

//===========================================================================================

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UIPANEL_UNIT ]
#include "log_macros.def"
//-------------------------------------------------------------------------------------------

TUIPanel::TUIPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color )
 : name(name),x(x),y(y),w(w),h(h),color(color)
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );

    this->ui = (TUI*)ui;
    this->ui->addpanel( this );

    visible = false;
    keyable = false;
    focused = false;

    listobject.clear( );
    focusobject = NULL;

    newresources( );
}
//-------------------------------------------------------------------------------------------

TUIPanel::~TUIPanel( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );

    while( !listobject.empty() ) delete listobject.front( );

    this->ui->removepanel( this );

    freeresources( );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::newresources( )
{FUNCTION_TRACE
    window = newwin( h, w, y, x ); if( !window ) Error( "newwin\n" );
    panel  = new_panel( window );  if( !panel ) Error( "new_panel\n" );

    for( TUIObjectListIt it = listobject.begin(); it != listobject.end(); ++it ) ((TUIObject*)(*it))->window = window;
}
//-------------------------------------------------------------------------------------------

void TUIPanel::freeresources( )
{FUNCTION_TRACE
    if( del_panel( panel ) == ERR ) Error( "del_panel\n" );
    if( delwin( window ) == ERR ) Error( "delwin\n" );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::addobject( TUIObject* object )
{FUNCTION_TRACE
    Debug( "%s %s\n", this->name.c_str(), object->name.c_str() );
    listobject.push_back( object );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::removeobject( TUIObject* object )
{FUNCTION_TRACE
    Debug( "%s %s\n", this->name.c_str(), object->name.c_str() );
    listobject.remove( object );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::setfocus( TUIObject* object )
{FUNCTION_TRACE
    Debug( "%s %s\n", this->name.c_str(), object->name.c_str() );
    if( focusobject ) focusobject->focused = false;
    focusobject = object;
    if( focusobject ) focusobject->focused = true;
}
//-------------------------------------------------------------------------------------------

void TUIPanel::changefocus( int key )
{FUNCTION_TRACE
    Debug( "%s %d\n", this->name.c_str(), key );

    TUIObject* f = focusobject;
    TUIObject* O = NULL;
    int DX = 99999;
    int DY = 99999;
    for( TUIObjectListIt it = listobject.begin(); it != listobject.end(); ++it )
    {
        TUIObject* o = (TUIObject*)(*it);
        if( ( o == f )|| !o->visible || !o->keyable ) continue;

        int dx = f->x - o->x; int adx = abs(dx);
        int dy = f->y - o->y; int ady = abs(dy);
        switch( key )
        {
            case KEY_UP    : if( (dy>0)&&( (+dy<DY)||((+dy==DY)&&(adx<DX)) ) ) { DX=adx;DY=+dy;O=o; } break;
            case KEY_DOWN  : if( (dy<0)&&( (-dy<DY)||((-dy==DY)&&(adx<DX)) ) ) { DX=adx;DY=-dy;O=o; } break;
            case KEY_LEFT  : if( (dx>0)&&( (ady<DY)||((ady==DY)&&(+dx<DX)) ) ) { DX=+dx;DY=ady;O=o; } break;
            case KEY_RIGHT : if( (dx<0)&&( (ady<DY)||((ady==DY)&&(-dx<DX)) ) ) { DX=-dx;DY=ady;O=o; } break;
        }
    }
    if( O )
    {
        TUIObject* prev = focusobject;
        setfocus( O );
        if( prev ) prev->draw( );
        O->draw( );
        O->cursor( );
        uirefresh( );
    }
}
//-------------------------------------------------------------------------------------------

void TUIPanel::clear( int x, int y, int w, int h )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    setcolor( window, color );
    for( int i = y; i < h; i++ )
        mvwprintw( window, i,x, "%*s", w, "" );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::drawobjects( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    for( TUIObjectListIt it = listobject.begin(); it != listobject.end(); ++it )
    {
        TUIObject* o = (TUIObject*)(*it);
        if( o->visible ) o->draw( );
    }
    if( focusobject ) focusobject->cursor( );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::doresize( int X, int Y, int W, int H )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    if( ( X >= 0 )&&( Y >= 0 )&&( W > 0 )&&( H > 0 ) )
    {
        x = X; y = Y; w = W; h = H;
        if( wresize( window, h, w ) == ERR ) Error( "wresize %s [%d/%d][%dx%d]\n", name.c_str(),x,y,w,h );
        if( move_panel( panel, y, x ) == ERR ) Error( "move_panel %s [%d/%d][%dx%d]\n", name.c_str(),x,y,w,h );
    }
    clear( 0,0,w,h );

    /*
    WINDOW* oldwindow = panel_window( panel );
    window = newwin( h, w, y, x );
    replace_panel( panel, window );
    delwin( oldwindow );
    for( TUIObjectListIt it = listobject.begin(); it != listobject.end(); ++it ) { TUIObject* o = (TUIObject*)(*it); o->window = window; }
    */
}
//-------------------------------------------------------------------------------------------

bool TUIPanel::doevent( int key, char* codes )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    bool result = true;
    if( focusobject ) result = focusobject->event( key,codes );

    if( result )
    {
        switch( key )
        {
            case KEY_UP    :
            case KEY_DOWN  :
            case KEY_LEFT  :
            case KEY_RIGHT : changefocus( key ); break;
        }
        result = event( key,codes );
        return result;
    }
    return true;
}
//-------------------------------------------------------------------------------------------

void TUIPanel::show( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    visible = true;
    if( show_panel( panel ) == ERR ) Error( "show_panel\n" );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::hide( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    visible = false;
    if( hide_panel( panel ) == ERR ) Error( "hide_panel\n" );
}
//-------------------------------------------------------------------------------------------

void TUIPanel::showfocused( TUIObject* object )
{FUNCTION_TRACE
    Debug( "%s %s\n", this->name.c_str(), object->name.c_str() );
    setfocus( object );
    ui->setfocus( this );
}
//-------------------------------------------------------------------------------------------

//===========================================================================================

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UIOBJECT_UNIT ]
#include "log_macros.def"
//-------------------------------------------------------------------------------------------

TUIObject::TUIObject( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color1, int color2, const std::string& text, int align )
: name(name),x(x),y(y),w(w),h(h),color1(color1),color2(color2),text(text),align(align)
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    this->uipanel = (TUIPanel*)uipanel;
    this->uipanel->addobject( this );
    window = uipanel->window;
    visible = true;
    keyable = false;
    focused = false;
    doalign( );
}
//-------------------------------------------------------------------------------------------

TUIObject::~TUIObject( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    this->uipanel->removeobject( this );
}
//-------------------------------------------------------------------------------------------

void TUIObject::settext( const std::string& text )
{FUNCTION_TRACE
    this->text = text;
    doalign( );
}
//-------------------------------------------------------------------------------------------

void TUIObject::doalign( )
{FUNCTION_TRACE
    if( align == UIL ) xalign = x; else
    if( align == UIC ) xalign = x + (w-utf8length(text))/2; else
    if( align == UIR ) xalign = x + w-utf8length(text);
    if( xalign < x ) xalign = x;
}

//-------------------------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UILABEL_UNIT ]
#include "log_macros.def"
//-------------------------------------------------------------------------------------------

TUILabel::TUILabel( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color, const std::string& text, int align )
: TUIObject( uipanel,name,x,y,w,h,color,0,text,align )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    keyable = false;
}
//-------------------------------------------------------------------------------------------

TUILabel::~TUILabel( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
}
//-------------------------------------------------------------------------------------------

void TUILabel::resize( int x, int y, int w, int h )
{FUNCTION_TRACE
    this->x = x; this->y = y; this->w = w; this->h = h;
    doalign( );
}
//-------------------------------------------------------------------------------------------

void TUILabel::draw( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    if( !visible ) return;
    setcolor( window, color1 );
    if( w > 0 ) if( mvwprintw( window, y,x, "%*s", w, "" ) == ERR ) {}//Error( "mvwprintw1 %s [%d/%d][%dx%d]\n", name.c_str(),x,y,w,h );

    if( w == 0 )
    {
        if( mvwprintw( window, y,xalign, "%s", text.c_str() ) == ERR ) Error( "mvwprintw2 %s [%d/%d][%dx%d]\n", name.c_str(),x,y,w,h );
    }
    else
    {
        int i = utf8indexsymbols( text, w+x-xalign );
        if( mvwprintw( window, y,xalign, "%s", text.substr(0,i).c_str() ) == ERR ) Error( "mvwprintw3 %s [%d/%d][%dx%d]\n", name.c_str(),x,y,w,h );
    }
}
//-------------------------------------------------------------------------------------------


#undef  UNIT
#define UNIT  g_DebugSystem.units[ UIBUTTON_UNIT ]
#include "log_macros.def"
//-------------------------------------------------------------------------------------------

TUIButton::TUIButton( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color1, int color2, const std::string& text, int align, void (*OnPress)( TUIButton* btn ) )
: TUIObject( uipanel,name,x,y,w,h,color1,color2,text,align ),OnPress(OnPress)
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    keyable = true;
}
//-------------------------------------------------------------------------------------------

TUIButton::~TUIButton( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
}
//-------------------------------------------------------------------------------------------

void TUIButton::resize( int x, int y, int w, int h )
{FUNCTION_TRACE
    this->x = x; this->y = y; this->w = w; this->h = h;
    doalign( );
}
//-------------------------------------------------------------------------------------------

void TUIButton::draw( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    if( !visible ) return;
    setcolor( window, ( focused && uipanel->isfocused() ) ? color2:color1 );
    for( int i = 0; i < h; i++ ) 	if( mvwprintw( window, y+i,x, "%*s", w, "" ) == ERR ) Error( "mvwprintw\n" );
    if( mvwprintw( window, y+(h-1)/2,xalign, "%s", text.c_str() ) == ERR ) Error( "mvwprintw\n" );
}
//-------------------------------------------------------------------------------------------

void TUIButton::cursor( )
{FUNCTION_TRACE
    curs_off( );
}
//-------------------------------------------------------------------------------------------

bool TUIButton::event( int key, char* /*codes*/ )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    if( ENTER(key) )
    {
        if( OnPress ) (*OnPress)( this );
        return false;
    }
    return true;
}
//-------------------------------------------------------------------------------------------

void TUIButton::press( )
{FUNCTION_TRACE
    if( OnPress ) (*OnPress)( this );
}
//-------------------------------------------------------------------------------------------


#undef  UNIT
#define UNIT  g_DebugSystem.units[ UIEDIT_UNIT ]
#include "log_macros.def"
//-------------------------------------------------------------------------------------------

TUIEdit::TUIEdit( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color1, int color2, const std::string& text )
: TUIObject( uipanel,name,x,y,w,h,color1,color2,text,UIL )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    keyable = true;
}
//-------------------------------------------------------------------------------------------

TUIEdit::~TUIEdit( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
}
//-------------------------------------------------------------------------------------------

void TUIEdit::resize( int x, int y, int w, int h )
{FUNCTION_TRACE
    this->x = x; this->y = y; this->w = w; this->h = h;
}
//-------------------------------------------------------------------------------------------

void TUIEdit::draw( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    if( !visible ) return;
    setcolor( window, ( focused && uipanel->isfocused() ) ? color2:color1 );
    if( mvwprintw( window, y,x, "%*s", w, "" ) == ERR ) Error( "mvwprintw\n" );
    if( mvwprintw( window, y,x, "%s", text.c_str() ) == ERR ) Error( "mvwprintw\n" );
}
//-------------------------------------------------------------------------------------------

void TUIEdit::cursor( )
{FUNCTION_TRACE
    pos = utf8length(text);
    wmove( window, y, pos + x );
    curs_on( );
}
//-------------------------------------------------------------------------------------------

bool TUIEdit::event( int key, char* codes )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );

    if( ESCAPE(key) ) return true;
    if( ENTER(key) ) return true;
    if( key == KEY_UP ) return true;
    if( key == KEY_DOWN ) return true;
    if( ( key >= KEY_F(0) )&&( key < KEY_F(64) ) ) return true;

    switch( key )
    {
        case KEY_LEFT: // Left
            if( pos == 0 ) return false;
            --pos;
            break;

        case KEY_RIGHT: // Right
            if( pos == utf8length(text) ) return false;
            ++pos;
            break;

        case KEY_HOME:
            pos = 0;
            break;

        case KEY_END:
            pos = utf8length(text);
            break;

        case KEY_BACKSPACE: // Backspace
        {
            if( pos == 0 ) return false;
            int i1 = utf8indexsymbols( text, pos-1 );
            int i2 = utf8indexsymbols( text, pos );
            text.erase( i1, i2-i1 );
            --pos; draw( );
        } break;

        case KEY_DC: // Delete
        {
            if( pos == utf8length(text) ) return false;
            int i1 = utf8indexsymbols( text, pos );
            int i2 = utf8indexsymbols( text, pos+1 );
            text.erase( i1, i2-i1 );
            draw( );
        } break;

        default:
        {
            if( utf8length(text) == w-1 ) return false;
            string s = codes; int l = utf8length(s);
            if( l <= 0 ) { errno=0; return false; }
            text.insert( utf8indexsymbols( text, pos ), s );
            pos += l; draw( );
        }
    }
    wmove( window, y, pos + x );
    uirefresh( );
    return false;
}
//-------------------------------------------------------------------------------------------

int TUIEdit::getint( )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    int value = 0;
    sscanf( text.c_str(), "%d", &value );
    return value;
}
//-------------------------------------------------------------------------------------------

void TUIEdit::setint( int value )
{FUNCTION_TRACE
    Debug( "%s\n", this->name.c_str() );
    text = stringformat( "%d", value );
    draw( );
}
//-------------------------------------------------------------------------------------------



