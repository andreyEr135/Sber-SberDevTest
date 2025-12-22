#include <iostream>
#include <fstream>
#include "debugsystem.h"
#include "main.h"
#include "test.h"
#include "ui.h"
#include "backpanel.h"
#include "menupanel.h"
#include "logpanel.h"
#include "sensorpanel.h"
#include "editpanel.h"
#include "dialogpanel.h"
#include "autostartpanel.h"
#include "hellopanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

THelloPanel* HelloPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ HELLOPANEL_UNIT ]
#include "log_macros.def"

// Прототипы функций обратного вызова
void OnHelloEditExit( std::string file, bool changed );
void OnPressFunctionalTestBtn( TUIButton* btn );
void OnPressStressTestBtn( TUIButton* btn );

//-------------------------------------------------------------------------------------------

THelloPanel::THelloPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
{
    FUNCTION_TRACE
    m_panelWidth = w;
    m_selectionPanel = new TTestSelectionPanel( ui, "SelectionPanel", 0, 0, m_panelWidth, h, color );
}

THelloPanel::~THelloPanel( )
{
    // Объект m_selectionPanel будет удален самой системой UI, так как он привязан к родителю или управляется отдельно
}

void THelloPanel::ShowPanel( )
{
    FUNCTION_TRACE
    BackPanel->HideMainLayout( );
    BackPanel->HideProgramLayout( );

    m_selectionPanel->RefreshAvailableTests( );
    UI->setfocus( m_selectionPanel );
}

void THelloPanel::HidePanel( )
{
    FUNCTION_TRACE
    m_selectionPanel->hide();
}

//===========================================================================================
// TTestSelectionPanel (TInisPanel)
//===========================================================================================

TTestSelectionPanel::TTestSelectionPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;
    m_functionalTestBtn = NULL;
    m_stressTestBtn = NULL;
    m_serialNumber = "";
    m_uuid = "";
}

TTestSelectionPanel::~TTestSelectionPanel( )
{
    FUNCTION_TRACE
}

void TTestSelectionPanel::resize( )
{
    FUNCTION_TRACE
    doresize( (COLS - w) / 2, (LINES - h) / 2, w, h );
}

void TTestSelectionPanel::draw( )
{
    FUNCTION_TRACE
    setcolor( window, color );
    box( window, 0, 0 );
    drawobjects( );
}

bool TTestSelectionPanel::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE
    if( ESCAPE(inputKey) ) return false;

    // Проверка блокировки редактирования
    bool isBlocked = (g_DebugSystem.conf->ReadInt("Main", "block") != 0);

    if( !isBlocked )
    {
        if( inputKey == KEY_F(4) )
        {
            if( focusobject )
            {
                if( focusobject->name == "FunctionalTestBtn" )
                    EditPanel->ShowFile( Test->inidir + "/sberpc_ft.ini", &OnHelloEditExit );
                else if( focusobject->name == "StressTestBtn" )
                    EditPanel->ShowFile( Test->inidir + "/sberpc_stress.ini", &OnHelloEditExit );
            }
        }
    }
    return true;
}

/**
 * @brief Поиск доступных конфигураций и создание кнопок
 */
void TTestSelectionPanel::RefreshAvailableTests( )
{
    FUNCTION_TRACE
    Test->SearchInis( );

    // Очищаем старые кнопки, если они были (предполагается механизм удаления объектов в UI)
    // Здесь создаем новые кнопки на основе списка INI
    FOR_EACH_ITER( TIniList, Test->IniList, it )
    {
        if (it->name == "sberpc_ft.ini")
        {
            m_functionalTestBtn = new TUIButton( this, "FunctionalTestBtn", 4, 2, w - 8, 5,
                                                 UIBLUE, UIGREEN, " Функциональное тестирование ", UIC, &OnPressFunctionalTestBtn );
            setfocus( m_functionalTestBtn );
        }

        if (it->name == "sberpc_stress.ini")
        {
            // Сдвигаем кнопку вниз, если первая уже создана
            int yPos = m_functionalTestBtn ? 14 : 2;
            m_stressTestBtn = new TUIButton( this, "StressTestBtn", 4, yPos, w - 8, 5,
                                             UIBLUE, UIGREEN, " Нагрузочное тестирование ", UIC, &OnPressStressTestBtn );

            if( !m_functionalTestBtn ) setfocus( m_stressTestBtn );
        }
    }
}

/**
 * @brief Получение серийного номера и UUID системы через dmidecode
 */
void TTestSelectionPanel::IdentifyHardware()
{
    m_serialNumber = "";
    m_uuid = "";
    string shellOutput = "";

    GetProcessDataTimeout("sudo dmidecode -t system", shellOutput, 500);

    if (!shellOutput.empty())
    {
        Tstrlist lines;
        int count = stringsplit(shellOutput, '\n', lines);
        for (int i = 0; i < count; ++i)
        {
            string line = lines[i];

            if (line.find("Serial Number:") != string::npos)
            {
                size_t pos = line.find("Serial Number:");
                m_serialNumber = trim(line.substr(pos + string("Serial Number:").length()));
            }

            if (line.find("UUID:") != string::npos)
            {
                size_t pos = line.find("UUID:");
                m_uuid = trim(line.substr(pos + string("UUID:").length()));
            }
        }
    }
}

//-------------------------------------------------------------------------------------------
// Обработчики событий кнопок
//-------------------------------------------------------------------------------------------

void OnPressFunctionalTestBtn( TUIButton* btn )
{
    FUNCTION_TRACE
    Test->SetLastIni( "sberpc_ft.ini" );
    HelloPanel->HidePanel( );
    AutoStartPanel->ResetCountdown();
    AutoStartPanel->ShowPanel( );
    UI->draw( );
}

void OnPressStressTestBtn( TUIButton* btn )
{
    FUNCTION_TRACE
    Test->SetLastIni( "sberpc_stress.ini" );
    HelloPanel->HidePanel( );
    AutoStartPanel->ResetCountdown();
    AutoStartPanel->ShowPanel( );
    UI->draw( );
}

void OnHelloEditExit( std::string file, bool changed )
{
    FUNCTION_TRACE
    HelloPanel->ShowPanel( );
}

