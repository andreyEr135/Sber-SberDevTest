#ifndef uiH
#define uiH
//-------------------------------------------------------------------------------------------

#include <pthread.h>

#include <list>
#include <iterator>

#include <ncurses.h>
#include <panel.h>

//-------------------------------------------------------------------------------------------

#define UIEVENT_INIT        10000
#define UIEVENT_EXIT        10001
#define UIEVENT_TERM_OPEN   10002
#define UIEVENT_TERM_CLOSE  10003
#define UIEVENT_TERM_RESIZE 10004
#define UIEVENT_USER        11000
#define UIEVENT_USER_MAX    12000

#define UIL -1
#define UIC  0
#define UIR +1

#define ESCAPE(k) (k==27)
#define ENTER(k)  ((k==10)||(k==13))

#define UILOCK DLOCK(UI->mutex)

class TUIPanel;
class TUIObject;

class TUI
{
    private:
        char* prevterm;
        volatile int do_event;
        Tevent event_done;
        void DoEvent( int event );

    protected:

        friend class TUIPanel;

        typedef std::list< TUIPanel* > TUIPanelList;
        typedef TUIPanelList::iterator TUIPanelListIt;

        TUIPanelList  listpanel;
        TUIPanel*     focuspanel;
        TUIPanelList  prevfocuslist;
        bool ftablock;
        bool fexit;
        void addpanel( TUIPanel* panel );
        void removepanel( TUIPanel* panel );
        bool tabpanel( );
        bool (*OnUIEvent)( TUI* ui, int event );
        void (*Hotkey)( int key, char* codes );

        typedef struct { char type[32]; char dev[32]; pid_t pid; } Tterminal;
        Tterminal    terminaldefault;
        Tterminal    terminalcurrent;
        SCREEN*      terminalscreen;
        FILE*        terminalfp;
        std::vector< TUIPanel* > panelstack;
        int          terminallines;
        int          terminalcols;

        //typedef std::list< int > Tevents;
        //Tevents events;

        bool opencurses( );
        void closecurses( );
        void newresources( );
        void freeresources( );

        //void KillTerminal( );
        void SwitchTerminal( Tterminal& terminal, pid_t pid = 0 );

        class Tkeynode;
        typedef std::list< Tkeynode* > Tnodelist;
        typedef Tnodelist::iterator    TnodelistIt;
        class Tkeynode
        {
            public:
                int code;
                int key;
                Tnodelist childs;
                Tkeynode( Tkeynode* parent, int code );
                ~Tkeynode( );
                Tkeynode* findchild( int code );
        };
        Tkeynode* keyroot;

    public:

        TUI( bool(*OnUIEvent)( TUI* ui, int event ), void(*Hotkey)( int key, char* codes ) );
        ~TUI( );

        void Run( );

        void resize( );
        void draw( );
        void setfocus( TUIPanel* panel );
        void setfocusprev( );
        void tablock( bool lock );
        void exit( );

        Tmutex mutex;
        void DoLock( int event );
};
//-------------------------------------------------------------------------------------------

class TUIPanel
{
    private:

    protected:

        friend class TUI;
        friend class TUIObject;

        TUI*           ui;
        WINDOW*        window;
        PANEL*         panel;
        bool           keyable;
        bool           focused;

        typedef std::list< TUIObject* > TUIObjectList;
        typedef TUIObjectList::iterator TUIObjectListIt;

        TUIObjectList  listobject;

        void addobject( TUIObject* object );
        void removeobject( TUIObject* object );
        void doresize( int X, int Y, int W, int H );
        bool doevent( int key, char* codes );
        void changefocus( int key );
        void clear( int x, int y, int w, int h );
        void drawobjects( );
        void newresources( );
        void freeresources( );
        virtual void resize( ) { }
        virtual void draw( ) { }
        virtual bool event( int key, char* codes ) { return true; }

    public:

        std::string    name;
        int            x,y,w,h;
        int            color;
        bool           visible;
        TUIObject*     focusobject;

        TUIPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color );
        virtual ~TUIPanel( );

        void show( );
        void hide( );

        void setfocus( TUIObject* object );
        bool isfocused( ) { return focused; }
        void showfocused( TUIObject* object );

};
//-------------------------------------------------------------------------------------------

class TUIObject
{
    protected:
        friend class TUI;
        friend class TUIPanel;

        TUIPanel*    uipanel;
        WINDOW*      window;
        bool         keyable;
        int          xalign;
        virtual void resize( int x, int y, int w, int h ) { }
        virtual void draw( ) { }
        virtual bool event( int key, char* codes ) { return true; }
        virtual void cursor( ) { }
        void doalign( );

    public:
        std::string  name;
        int          x,y,w,h;
        int          color1,color2;
        bool         visible;
        std::string  text;
        int          align;
        bool         focused;

        TUIObject( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color1, int color2, const std::string& text, int align );
        virtual ~TUIObject( );

        void settext( const std::string& text );
};
//-------------------------------------------------------------------------------------------

class TUILabel : public TUIObject
{
    protected:

    public:
        TUILabel( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color, const std::string& text, int align );
        virtual ~TUILabel( );

        virtual void resize( int x, int y, int w, int h );
        virtual void draw( );
};
//-------------------------------------------------------------------------------------------

class TUIButton : public TUIObject
{
    protected:
        virtual bool event( int key, char* codes );
        void (*OnPress)( TUIButton* btn );

    public:
        TUIButton( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color1, int color2, const std::string& text, int align, void (*OnPress)( TUIButton* btn ) );
        virtual ~TUIButton( );

        virtual void resize( int x, int y, int w, int h );
        virtual void draw( );
        virtual void cursor( );

        void press( );
};
//-------------------------------------------------------------------------------------------

class TUIEdit : public TUIObject
{
    protected:
        virtual bool event( int key, char* codes );
        int pos;

    public:
        TUIEdit( const TUIPanel* uipanel, const std::string& name, int x, int y, int w, int h, int color1, int color2, const std::string& text );
        virtual ~TUIEdit( );

        virtual void resize( int x, int y, int w, int h );
        virtual void draw( );
        virtual void cursor( );

        int  getint( );
        void setint( int value );
};
//-------------------------------------------------------------------------------------------

int  curs_on( );
int  curs_off( );
void setcolor( WINDOW* window, int color );
void drawlineH( WINDOW* window, int x, int y, int w );
void drawlineV( WINDOW* window, int x, int y, int h );
void uirefresh( );

extern TUI* UI;
extern bool KEYTRACE;

//-------------------------------------------------------------------------------------------
#endif

