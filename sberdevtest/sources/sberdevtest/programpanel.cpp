#include <iostream>
#include <fstream>
#include "debugsystem.h"
#include "main.h"
#include "test.h"
#include "ui.h"
#include "hellopanel.h"
#include "backpanel.h"
#include "dialogpanel.h"
#include "programpanel.h"
#include "firmwaredata.h"
#include "tcpclient.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TProgramPanel* ProgramPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ PROGRAMPANEL_UNIT ]
#include "log_macros.def"

// Прототипы коллбэков
void OnFirmwareApplyClick( TUIButton* btn );
void OnFirmwareConfirmDialog( TUIButton* btn );
void OnFirmwareSuccessDialog( TUIButton* btn );

//-------------------------------------------------------------------------------------------

TProgramPanel::TProgramPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
{
    FUNCTION_TRACE
    m_panelWidth = w;
    m_inputPanel = new TDeviceFirmwarePanel( ui, "FirmwareInputPanel", 0, 0, m_panelWidth, h, color );
}

TProgramPanel::~TProgramPanel( )
{
    // Система UI сама удалит m_inputPanel, если она привязана к родителю
}

void TProgramPanel::ShowPanel( )
{
    FUNCTION_TRACE
    UI->setfocus( m_inputPanel );
}

void TProgramPanel::HidePanel( )
{
    FUNCTION_TRACE
    m_inputPanel->hide();
}

//===========================================================================================
// TDeviceFirmwarePanel
//===========================================================================================

TDeviceFirmwarePanel::TDeviceFirmwarePanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = true;
    m_firmwareLogic = new FirmwareData();

    // Заголовки и поля ввода
    new TUILabel( this, "TitleLabel",  2, 0, 0, 0, color, " Программирование устройства ", UIL );

    new TUILabel( this, "SerialLabel", 2, 2, 0, 0, color, " Серийный номер ", UIL );
    m_serialEdit = new TUIEdit( this, "SerialEdit", 2, 4, w - 5, 0, -color, UIGREEN, "" );
    new TUILabel( this, "UuidLabel",   2, 8, 0, 0, color, " UUID ", UIL );
    m_uuidEdit   = new TUIEdit( this, "UuidEdit",   2, 10, w - 5, 0, -color, UIGREEN, "" );

    new TUILabel( this, "MacLabel",    2, 14, 0, 0, color, " MAC адрес ", UIL );
    m_macEdit    = new TUIEdit( this, "MacEdit",    2, 16, w - 5, 0, -color, UIGREEN, "" );

    m_setBtn = new TUIButton( this, "SetBtn", 4, 20, w - 8, 5, UIBLUE, UIGREEN, " Настроить ", UIC, &OnFirmwareApplyClick );

    // Инициализация данных
    if (m_firmwareLogic)
    {
        m_macEdit->settext(trim(m_firmwareLogic->getMac()));
    }

    BackPanel->StatusLabel->text = "Прошивка";
    setfocus( m_serialEdit );
}

TDeviceFirmwarePanel::~TDeviceFirmwarePanel( )
{
    FUNCTION_TRACE
    if (m_firmwareLogic) delete m_firmwareLogic;
}

void TDeviceFirmwarePanel::resize( )
{
    FUNCTION_TRACE
    doresize( (COLS - w) / 2, (LINES - h) / 2, w, h );
}

void TDeviceFirmwarePanel::draw( )
{
    FUNCTION_TRACE
    setcolor( window, color );
    box( window, 0, 0 );

    // Разделительные линии
    drawlineH( window, 0, 6, w );
    drawlineH( window, 0, 12, w );
    drawlineH( window, 0, 18, w );

    drawobjects( );
}

bool TDeviceFirmwarePanel::event( int inputKey, char* scanCodes )
{
    FUNCTION_TRACE
    if( ESCAPE(inputKey) ) return false;
    return true;
}

//===========================================================================================
// Функции обработки событий
//===========================================================================================

/**
 * @brief Нажатие на кнопку "Настроить"
 */
void OnFirmwareApplyClick( TUIButton* btn )
{
    FUNCTION_TRACE
    string serial = ProgramPanel->m_inputPanel->m_serialEdit->text;
    string uuid   = ProgramPanel->m_inputPanel->m_uuidEdit->text;
    string mac    = ProgramPanel->m_inputPanel->m_macEdit->text;

    if (serial.empty() || uuid.empty() || mac.empty())
    {
        DialogPanel->ShowMessage( UIRED, "60,9{}{Укажите серийный номер, UUID, MAC-адрес!}{Ok}{Ok}", &OnPressOkDialogBtn );
    }
    else
    {
        string question = "Желаете прошить устройство?";
        DialogPanel->ShowMessage( UIRED, stringformat("60,9{}{%s}{Да|Нет}{Нет}", question.c_str()), &OnFirmwareConfirmDialog );
    }
}

/**
 * @brief Диалог подтверждения прошивки
 */
void OnFirmwareConfirmDialog( TUIButton* btn )
{
    FUNCTION_TRACE
    string answer = trim(btn->text);

    if( answer == "Да" )
    {
        string serial = ProgramPanel->m_inputPanel->m_serialEdit->text;
        string uuid   = ProgramPanel->m_inputPanel->m_uuidEdit->text;
        string mac    = ProgramPanel->m_inputPanel->m_macEdit->text;

        BackPanel->StatusLabel->text = "Идет прошивка, ожидайте...";
        DialogPanel->Close( );

        if (ProgramPanel->m_inputPanel->m_firmwareLogic->firmware(serial, uuid, mac))
        {
            BackPanel->StatusLabel->text = "Устройство прошито";
            DialogPanel->ShowMessage( UIBLUE, "45,9{}{Устройство успешно прошито!}{Ok}{Ok}", &OnFirmwareSuccessDialog );
        }
        else
        {
            BackPanel->StatusLabel->text = "Ошибка при прошивке";
            DialogPanel->ShowMessage( UIRED, "45,9{}{Не удалось прошить устройство!}{Ok}{Ok}", &OnPressOkDialogBtn );
        }
    }
    else
    {
        DialogPanel->Close( );
    }
}

/**
 * @brief Действие после успешной прошивки
 */
void OnFirmwareSuccessDialog( TUIButton* btn )
{
    DialogPanel->Close();
    ProgramPanel->HidePanel();

    // Возвращаемся в приветственную панель и обновляем данные о железе
    HelloPanel->ShowPanel();
    HelloPanel->m_selectionPanel->IdentifyHardware();

    // Отправляем новый серийный номер по сети
    std::string serial = HelloPanel->m_selectionPanel->m_serialNumber;
    std::string sendMsg = stringformat("#sn#%s#", serial.c_str());
    Tcp->sendData(sendMsg.c_str(), sendMsg.length());
}
