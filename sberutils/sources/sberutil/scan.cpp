//-------------------------------------------------------------

#include "debugsystem.h"

#include "scan.h"

//---------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UNIT_SCAN ]
#include "log_macros.def"

using namespace std;

//---------------------------------------------------------------------------

//===========================================================================

Tscan::Tscan( std::string modules ) : modules(modules)
{FUNCTION_TRACE
    pci  = ( modules.find( "pci"  ) != string::npos ) ? new TmemTable( PCI_TBL  ) : NULL;
    usb  = ( modules.find( "usb"  ) != string::npos ) ? new TmemTable( USB_TBL  ) : NULL;
    blk  = ( modules.find( "blk"  ) != string::npos ) ? new TmemTable( BLK_TBL  ) : NULL;
    net  = ( modules.find( "net"  ) != string::npos ) ? new TmemTable( NET_TBL  ) : NULL;
}
//---------------------------------------------------------------------------

Tscan::~Tscan( )
{FUNCTION_TRACE
	xdelete( pci  );
	xdelete( usb  );
	xdelete( blk  );
	xdelete( net  );
}
//---------------------------------------------------------------------------

bool Tscan::open( std::string bindb )
{FUNCTION_TRACE
	try
	{
		close( );

        string run = g_DebugSystem.fullpath( bindb ) + " " + modules;
		Trace( "" CLRCYN( "run: %s" ) "\n", run.c_str() );
		int rc = ExecuteProcessSilent( run.c_str() );
        if( rc != 0 ) THROW( "exitcode %d", rc );
		if( pci  ) if( !pci ->open( ) ) THROW( pci ->error );
		if( usb  ) if( !usb ->open( ) ) THROW( usb ->error );
		if( blk  ) if( !blk ->open( ) ) THROW( blk ->error );
		if( net  ) if( !net ->open( ) ) THROW( net ->error );
	}
	CATCH { errstr = e.what(); return false; }
	return true;
}
//---------------------------------------------------------------------------

void Tscan::close( )
{FUNCTION_TRACE
	if( pci  ) pci ->close( );
	if( usb  ) usb ->close( );
	if( blk  ) blk ->close( );
	if( net  ) net ->close( );
}
//---------------------------------------------------------------------------

TtblRecord Tscan::pci_rec( std::string vendevs )
{FUNCTION_TRACE
	if( !DBOPEN( pci ) ) return recNULL;
    FOR_EACH_TOKEN( vendevs, '|', list, s )
	{
		WORD vd[2]; if( !vendevscan( *s, vd ) ) continue;
		TtblRecord r; pci->find( r, PCI_VENDEV, vd );
		if( r ) return r;
	}
	return recNULL;
}
//---------------------------------------------------------------------------

TtblRecord Tscan::usb_rec( std::string usbports )
{FUNCTION_TRACE
	if( !DBOPEN( usb ) ) return recNULL;
    FOR_EACH_TOKEN( usbports, '|', list, p )
	{
		TtblRecord r; usb->find( r, USB_BUSPORT, *p );
		if( r ) return r;
	}
	return recNULL;
}
//---------------------------------------------------------------------------

TFindUsb Tscan::fUsb( std::string usbports )
{FUNCTION_TRACE
	try
	{
		TtblRecord u = usb_rec( usbports );
        if( !u ) return TFindUsb( USB_NONE );

		string usbsyspath = string( u.str(USB_SYSPATH) ) + "/" + u.str(USB_BUSPORT) + ":";

		if( blk )
		{
			if( !DBOPEN( blk ) ) THROW( "blk not opened" );
			for( TtblRecord b = blk->first(); b; ++b )
				if( strstr( b.str(BLK_SYSPATH), usbsyspath.c_str() ) )
				{
                    if( *((uint64_t*)b.ptr(BLK_SIZE)) > 0 ) return TFindUsb( USB_FLASH, u, b.str(BLK_DEV) );
                    else return TFindUsb( USB_NONE );
				}
		}

        return TFindUsb( USB_UNKNOWN, u );
	}
	CATCH { errstr = e.what(); }
    return TFindUsb( USB_NULL );
}
//---------------------------------------------------------------------------

TFindUsb Tscan::fUsb( TtblRecord controller, std::string _usbport )
{FUNCTION_TRACE
	try
	{
		if( !controller ) THROW( "controller NULL" );
		if( _usbport.empty() ) THROW( "_usbports NULL" );

		string usbports;
		for( TtblRecord u = usb->first(); u; ++u )
		{
			string busport = u.str(USB_BUSPORT);
			if( busport.substr( 0, 3 ) != "usb" ) continue;
			if( !strstr( u.str(USB_SYSPATH), controller.str(PCI_BUSSTR) ) ) continue;
			usbports += busport.substr( 3 ) + "-" + _usbport + "|";
		}
        return fUsb( usbports );
	}
	CATCH { errstr = e.what(); }
    return TFindUsb( USB_NULL );
}
//---------------------------------------------------------------------------

TFindUsb Tscan::fUsb( std::string controller_vendev, std::string _usbport )
{
    return fUsb( pci_rec( controller_vendev ), _usbport );
}
//---------------------------------------------------------------------------

bool sort_hosts( const std::string& a, const std::string& b ) { return str2int(a) < str2int(b); }

//---------------------------------------------------------------------------

TFindDisk Tscan::fDisk( TtblRecord controller, uint id )
{FUNCTION_TRACE
	try
	{
		if( !DBOPEN( blk ) ) THROW( "blk not opened" );
		if( !controller ) THROW( "controller NULL" );

		Tstrlist hosts;
		string path = string() + "/sys/bus/pci/devices/" + controller.str(PCI_BUSSTR);
		dxfordir( dir, path, d )
		{
			if( d->d_type != DT_DIR ) continue;
			if( strstr( d->d_name, "host" ) )
				hosts.push_back( d->d_name );

			if( strstr( d->d_name, "ata" ) )
			{
				dxfordir( ata, path + "/" + d->d_name, a )
					if( strstr( a->d_name, "host" ) )
						hosts.push_back( a->d_name );
			}
		}

		std::sort( hosts.begin(), hosts.end(), sort_hosts );

		if( id >= 0 && id < hosts.size() )
		{
			for( TtblRecord b = blk->first(); b; ++b )
			{
				if( strstr( b.str(BLK_SYSPATH), controller.str(PCI_BUSSTR) )
				 && strstr( b.str(BLK_SYSPATH), hosts[ id ].c_str() ) )
                    return TFindDisk( DISK_DEVICE, b );
			}
		}

        return TFindDisk( DISK_NONE );
	}
	CATCH { errstr = e.what(); }
    return TFindDisk( DISK_NULL );
}
//---------------------------------------------------------------------------





// FINDUSB ===========================================================================

TFindUsb::TFindUsb( )
{
	clear();
}

TFindUsb::TFindUsb( Tusb_type type, TtblRecord u, char* dev )
{
	clear();
	record = u;
	this->type = type;
	if( !u ) return;
	int i = u.get( USB_BCD );
	bcd[0] = (i>>8)&0x0F;
	bcd[1] = (i>>4)&0x0F;
	strncpy( busport, u.str(USB_BUSPORT), sizeof(busport)-1 );
	strncpy( driver, u.str(USB_DRIVER), sizeof(driver)-1 );
	if( dev ) strncpy( device, dev, sizeof(device)-1 );
}

void TFindUsb::clear( )
{
	record.null();
    type = USB_NULL;
	bcd[0] = bcd[1] = 0;
	memset( busport, 0, sizeof(busport) );
	memset( driver, 0, sizeof(driver) );
	memset( device, 0, sizeof(device) );
}

TFindUsb:: operator bool( )
{
    return ( type != USB_NONE )&&( type != USB_NULL );
}

std::string TFindUsb::info( )
{
	string spd = stringformat( "%d.%d", bcd[0],bcd[1] );
	switch( type )
	{
        case USB_NULL    : return string() + CLRRED( "null" );
        case USB_NONE    : return "";
        case USB_UNKNOWN : return string() + CLRYLW( "[" + driver + "]" );
        case USB_FLASH   : return string() + CLRGRN( "flash" )  + "|" + device + "|" + spd;
	}
	return "unknown type";
}
//---------------------------------------------------------------------------




// FINDDISK ===========================================================================

TFindDisk::TFindDisk( )
{
	clear( );
}

TFindDisk::TFindDisk( Tdisk_type type, TtblRecord b )
{
	clear( );
	record = b;
	this->type = type;
	if( !b ) return;
	strncpy( device, b.str(BLK_DEV), sizeof(device)-1 );

}

void TFindDisk::clear( )
{
	record.null();
    type = DISK_NULL;
	memset( device, 0, sizeof(device) );
	memset( speed, 0, sizeof(speed) );
}

TFindDisk:: operator bool( )
{
    return ( type != DISK_NONE )&&( type != DISK_NULL );
}

std::string TFindDisk::info( )
{
	switch( type )
	{
        case DISK_NULL    : return string() + CLRRED( "NULL" );
        case DISK_NONE    : return "";
        case DISK_DEVICE  : return string() + CLR(f_GREEN)+ device +CLR0;
	}
	return "unknown type";
}

//---------------------------------------------------------------------------





