#--------------------------------------- cpu
set( X CPU )
if( ${X} )
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O0 -DPOSIX -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -use-msasm -mmmx -msse -msse2 -msse3" )
	EXECUTABLE( ${X} sources/cpu )
	LIBRARIES ( ${X} rt pthread sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- hdd
set( X HDD )
if( ${X} )
	EXECUTABLE( ${X} sources/hdd )
	LIBRARIES ( ${X} rt m sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- lan
set( X LAN )
if( ${X} )
	EXECUTABLE( ${X} sources/lan )
	LIBRARIES ( ${X} sberutils pthread )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- mem
set( X MEM )
if( ${X} )
	EXECUTABLE( ${X} sources/mem )
	LIBRARIES ( ${X} rt sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- sound
set( X SOUND )
if( ${X} )
	EXECUTABLE( ${X} sources/sound )
	LIBRARIES ( ${X} pthread sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
endif( ${X} )

#--------------------------------------- video
set( X VIDEO )
if( ${X} )
	EXECUTABLE( ${X} sources/video )
	LIBRARIES ( ${X} pthread sberutils usb-1.0)
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- scan_dev
set( X SCAN_DEV )
if( ${X} )
	EXECUTABLE( ${X} sources/scan_dev )
	LIBRARIES ( ${X} pci sberutils )
#	DXVERSION ( ${X} )
#	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- wifi
set( X WIFI )
if( ${X} )
	EXECUTABLE( ${X} sources/wifi )
	LIBRARIES ( ${X} sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- bluetooth
set( X BT )
if( ${X} )
	EXECUTABLE( ${X} sources/bluetooth )
	LIBRARIES ( ${X} sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )