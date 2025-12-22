cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  if(POLICY CMP0037)
     cmake_policy(SET CMP0037 OLD)
  endif()
endif()

project(sberutils)

set( SBERUTILS   1 )
set( VERSION     1 )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
set( EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../bin )
set( LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../bin )

if ( CMAKE_CROSSCOMPILING )
	# Yocto build system complains about build path included in RPATH for libdxutil.so.0.0
	message(WARNING "Libdir is [${libdir}]")
	set(CMAKE_SKIP_RPATH TRUE)
endif ()

#---------------------------------------------------------------------------

include( ${PROJECT_SOURCE_DIR}/../scripts/_macros.cmake )

set( VERSION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/version )

include_directories( ${PROJECT_SOURCE_DIR}/include )

link_directories( ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pipe -Wall" ) 

#--------------------------------------- sberutils
if( ${SBERUTILS} )
	set( SBERUTILS_NAME sberutils )
	set( SBERUTILS_TYPE SHARED )
	set( SBERUTILS_SRC_DIR ${PROJECT_SOURCE_DIR}/sources/sberutil )
	GET_SOURCE_LIST( ${SBERUTILS_SRC_DIR} SBERUTILS_SRC_LIST )
	add_library( ${SBERUTILS_NAME} ${SBERUTILS_TYPE} ${SBERUTILS_SRC_LIST} )
	target_link_libraries( ${SBERUTILS_NAME} rt pthread util )
	if (NOT CMAKE_CROSSCOMPILING)
		# Yocto build system complains about stripped libraries (it strips them itself).
		# Hence, for cross-compiling (assuming Yocto) we don't strip the lib.
		#add_custom_command( TARGET ${DXUTIL_NAME} POST_BUILD COMMAND ${CMAKE_STRIP} lib${DXUTIL_NAME}.so.${DXUTIL_VERSION} WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
		add_custom_command( TARGET ${SBERUTILS_NAME} POST_BUILD COMMAND ${CMAKE_STRIP} lib${SBERUTILS_NAME}.so WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
	endif ()
endif( ${SBERUTILS} )

#--------------------------------------- version
set( X VERSION )
if( ${X} )
	EXECUTABLE( ${X} sources/version )
	LIBRARIES ( ${X} sberutils )
	STRIP     ( ${X} )
	ROOT      ( ${X} )
	DEPEND    ( ${X} sberutils )
endif( ${X} )

#---------------------------------------------------------------------------

