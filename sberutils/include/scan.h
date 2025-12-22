#ifndef scanH
#define scanH
//---------------------------------------------------------------------------

#include "sysutils.h"
#include "memtbl.h"
#include "devscan.h"

//---------------------------------------------------------------------------

#define DBOPEN(db) (db&&db->opened())

//---------------------------------------------------------------------------

typedef enum { USB_NULL, USB_NONE, USB_UNKNOWN, USB_FLASH } Tusb_type;

struct TFindUsb
{
	TtblRecord  record;
    Tusb_type type;
	BYTE        bcd[2];
	char        busport[16];
	char        driver [16];
	char        device [16];
    TFindUsb( );
    TFindUsb( Tusb_type type, TtblRecord u = recNULL, char* dev=NULL );
	void clear( );
	operator bool( );
	std::string info( );
};
//---------------------------------------------------------------------------

typedef enum { DISK_NULL, DISK_NONE, DISK_DEVICE } Tdisk_type;

struct TFindDisk
{
	TtblRecord   record;
    Tdisk_type type;
	char         device[16];
	char         speed [16];
    TFindDisk( );
    TFindDisk( Tdisk_type type, TtblRecord b = recNULL );
	void clear( );
	operator bool( );
	std::string info( );
};
//---------------------------------------------------------------------------


class Tscan
{
protected:
	std::string modules;
	std::string errstr;

public:
    Tscan( std::string modules );
    ~Tscan( );

	bool open( std::string bindb = "./devscan" );
	void close( );

	char* error( ) { return (char*)errstr.c_str(); }

	TtblRecord pci_rec( std::string vendevs );
	TtblRecord usb_rec( std::string usbports );

    TFindUsb  fUsb ( std::string usbports );
    TFindUsb  fUsb ( TtblRecord controller, std::string _ubsport );
    TFindUsb  fUsb ( std::string controller_vendev, std::string _usbport );

    TFindDisk fDisk( TtblRecord controller, uint id );

    TmemTable* pci;
    TmemTable* usb;
    TmemTable* blk;
    TmemTable* nand;
    TmemTable* net;
    TmemTable* com;
    TmemTable* can;
    TmemTable* gpio;
};
//---------------------------------------------------------------------------

#endif

