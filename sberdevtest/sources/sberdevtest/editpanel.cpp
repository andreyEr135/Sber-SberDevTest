#include "debugsystem.h"
#include "main.h"
#include "dialogpanel.h"
#include "editpanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TEditPanel* EditPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ EDITPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TEditPanel::TEditPanel( const TUI* ui, const std::string name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;

    m_topLabel    = new TUILabel( this, "TopLabel", 0, 0, 0, 0, -color, "", UIL );
    m_bottomLabel = new TUILabel( this, "BottomLabel", 0, 0, 0, 0, -color, "F2-Сохранить | F3-Сохранить как | Esc-Закрыть", UIL );
    m_memo        = new TUIMemo( this, "Memo", 0, 0, 0, 0, color );

    m_onExitCallback = NULL;
    m_fileWasModified = false;
}

TEditPanel::~TEditPanel( )
{
    FUNCTION_TRACE
}

//-------------------------------------------------------------------------------------------

void TEditPanel::resize( )
{
    FUNCTION_TRACE
    doresize( 0, 0, COLS, LINES );
    m_topLabel->resize( 0, 0, w, 1 );
    m_bottomLabel->resize( 0, h - 1, w, 1 );
    m_memo->resize( 0, 1, w, h - 2 );
}

void TEditPanel::draw( )
{
    FUNCTION_TRACE
    drawobjects( );
}

//-------------------------------------------------------------------------------------------
// Коллбэки диалогов
//-------------------------------------------------------------------------------------------

void OnPressEditEscDialogBtn( TUIButton* btn )
{
    FUNCTION_TRACE
    DialogPanel->Close( );
    string answer = trim(btn->text);

    if( answer == "Да" ) {
        if( EditPanel->SaveFile( ) ) EditPanel->ClosePanel( );
    }
    else if( answer == "Нет" ) {
        EditPanel->ClosePanel( );
    }
}

void OnPressEditSaveDialogBtn( TUIButton* btn )
{
    FUNCTION_TRACE
    DialogPanel->Close( );
    string answer = trim(btn->text);
    if( answer == "Да" ) EditPanel->SaveFile( );
}

void OnPressEditSaveAsDialogBtn( TUIButton* btn )
{
    FUNCTION_TRACE
    DialogPanel->Close( );
    string answer = trim(btn->text);
    if( answer == "OK" ) EditPanel->SaveFileAs( DialogPanel->GetInputText() );
}

//-------------------------------------------------------------------------------------------

bool TEditPanel::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE

    if( ESCAPE(inputKey) )
    {
        // Если текст не менялся - закрываем сразу
        if( !m_memo->IsModified() )
        {
            ClosePanel( );
        }
        else
        {
            DialogPanel->ShowMessage( UIBLUE,
                stringformat( "40,9{Закрыть %s}{Сохранить изменения?}{Да|Нет|Отмена}{Отмена}", m_fileName.c_str() ),
                &OnPressEditEscDialogBtn
            );
        }
    }

    if( inputKey == KEY_F(2) )
    {
        DialogPanel->ShowMessage( UIBLUE,
            stringformat( "40,9{%s}{Сохранить изменения?}{Да|Нет}{Да}", m_fileName.c_str() ),
            &OnPressEditSaveDialogBtn
        );
    }

    if( inputKey == KEY_F(3) )
    {
        DialogPanel->ShowInput( UIBLUE,
            stringformat( "40,10{Сохранить как}{Введите имя файла}{OK|Отмена}{%s}", m_fileName.c_str() ),
            &OnPressEditSaveAsDialogBtn
        );
    }

    return true;
}

//-------------------------------------------------------------------------------------------

void TEditPanel::ShowFile( const std::string fullPath, void (*onExit)( std::string file, bool changed ) )
{
    FUNCTION_TRACE
    Trace( "Edit show: %s\n", fullPath.c_str() );

    m_fileName        = getfile( fullPath );
    m_directoryPath   = getpath( fullPath );
    m_fileWasModified = false;
    m_onExitCallback  = onExit;

    m_topLabel->text = m_fileName;

    bool loadSuccess = m_memo->LoadFromFile( m_directoryPath + "/" + m_fileName );
    if( !loadSuccess )
    {
        DialogPanel->ShowMessage( UIRED,
            stringformat( "40,9{}{Ошибка открытия %s}{Ok}{Ok}", m_fileName.c_str() ),
            &OnPressOkDialogBtn
        );
        return;
    }

    ui->tablock( true );
    showfocused( m_memo );
}

//-------------------------------------------------------------------------------------------

void TEditPanel::ClosePanel( )
{
    FUNCTION_TRACE
    hide( );
    ui->tablock( false );
    ui->setfocusprev( );

    if( m_onExitCallback )
        (*m_onExitCallback)( m_fileName, m_fileWasModified );
}

//-------------------------------------------------------------------------------------------

bool TEditPanel::SaveFile( )
{
    FUNCTION_TRACE
    string fullPath = m_directoryPath + "/" + m_fileName;
    bool saveSuccess = m_memo->SaveToFile( fullPath );

    if( !saveSuccess )
    {
        DialogPanel->ShowMessage( UIRED,
            stringformat( "40,9{}{Ошибка записи в %s}{Ok}{Ok}", m_fileName.c_str() ),
            &OnPressOkDialogBtn
        );
    }

    m_fileWasModified = saveSuccess;
    return saveSuccess;
}

//-------------------------------------------------------------------------------------------

bool TEditPanel::SaveFileAs( std::string newFileName )
{
    FUNCTION_TRACE
    m_fileName = newFileName;
    m_topLabel->text = m_fileName;

    bool result = SaveFile( );

    draw( );
    uirefresh( );
    return result;
}
