cmake_minimum_required( VERSION 2.4 )
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  if(POLICY CMP0037)
     cmake_policy(SET CMP0037 OLD)
  endif()
endif()

project( SberDevTest )

set( SBERDEVTEST 1 )
set( SENSORSELF  1 )
set( SENSOR      1 )
set( DEVSCAN     1 )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
set( EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../bin )
set( LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../bin )

#---------------------------------------------------------------------------

include( ${PROJECT_SOURCE_DIR}/../scripts/_macros.cmake )

set( VERSION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/version )

set( SBERUTILS_DIR ${PROJECT_SOURCE_DIR}/../sberutils )

include_directories( ${PROJECT_SOURCE_DIR}/include ${SBERUTILS_DIR}/include )

link_directories( ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )

#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pipe -Wall -Wno-literal-suffix -Wno-deprecated-declarations" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pipe -Wall" )
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w" )

#--------------------------------------- sberdevtest
set( X SBERDEVTEST )
if( ${X} )
	EXECUTABLE( ${X} sources/sberdevtest )
	LIBRARIES ( ${X} rt sberutils pthread panelw ncursesw tinfo )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
	file( MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ini )
	file( MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/log )
	file( MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/modules )
endif( ${X} )

#--------------------------------------- sensorself
set( X SENSORSELF )
if( ${X} )
	EXECUTABLE( ${X} sources/sensorself )
	LIBRARIES ( ${X} sberutils )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
endif( ${X} )

#--------------------------------------- sensor
set( X SENSOR )
if( ${X} )
	EXECUTABLE( ${X} sources/sensor )
	LIBRARIES ( ${X} sberutils pthread )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#--------------------------------------- devscan
set( X DEVSCAN )
if( ${X} )
	EXECUTABLE( ${X} sources/devscan )
	LIBRARIES ( ${X} sberutils pci usb-1.0 )
	DXVERSION ( ${X} )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
endif( ${X} )

#---------------------------------------------------------------------------

include( ${PROJECT_SOURCE_DIR}/Device.cmake )

include( ${PROJECT_SOURCE_DIR}/Tests.cmake )

#---------------------------------------------------------------------------

