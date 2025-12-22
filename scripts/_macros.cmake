#---------------------------------------------------------------------------

MACRO( GET_SOURCE_LIST path srclist )
	#message( "path=${path} srclist=${srclist} " )
	file( READ ${path}/_sourcelist STR )
	#message( "STR=${STR}" )
	STRING(REPLACE "\n" ";" LST ${STR} )
	#message( "LST=${LST}" )
	FOREACH( i ${LST} )
		list( APPEND ${srclist} ${path}/${i} )
		#message( "i=${i}" )
	ENDFOREACH( i )
	#message( "${srclist} = ${${srclist}}" )
ENDMACRO( GET_SOURCE_LIST )

#---------------------------------------------------------------------------

MACRO( EXECUTABLE X sourcepath )
	set( ${X}_SOURCE_DIR ${PROJECT_SOURCE_DIR}/${sourcepath} ) # dir
	file( READ ${${X}_SOURCE_DIR}/_bin STR )
	#message( "STR=[${STR}]" )
	STRING(REGEX REPLACE "[\n\t\r ]" "" ${X}_NAME ${STR} )     # name
	#message( "NAME=[${X}_NAME]" )
	GET_SOURCE_LIST( ${${X}_SOURCE_DIR} ${X}_SOURCE_LIST )     # sourcelist
	add_executable( ${${X}_NAME} ${${X}_SOURCE_LIST} )         # exe
ENDMACRO( EXECUTABLE )

#---------------------------------------------------------------------------

MACRO( COMPFLAGS X flags )
	set_target_properties( ${${X}_NAME} PROPERTIES COMPILE_FLAGS "${flags}" )
ENDMACRO( COMPFLAGS )

#---------------------------------------------------------------------------

MACRO( INCLUDEDIR X dir )
	include_directories( ${${X}_SOURCE_DIR}/${dir} )
ENDMACRO( INCLUDEDIR )

#---------------------------------------------------------------------------

MACRO( LIBRARIES X )
	target_link_libraries( ${${X}_NAME} ${ARGN} )
ENDMACRO( LIBRARIES )

#---------------------------------------------------------------------------

MACRO( DXVERSION X )
	if (NOT CMAKE_CROSSCOMPILING)
		add_custom_command( TARGET ${${X}_NAME} POST_BUILD COMMAND ${VERSION} ${${X}_SOURCE_DIR} WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
	endif ()
ENDMACRO( DXVERSION )
#---------------------------------------------------------------------------

MACRO( STRIP X )
	if (NOT CMAKE_CROSSCOMPILING)
		add_custom_command( TARGET ${${X}_NAME} POST_BUILD COMMAND ${CMAKE_STRIP} ${${X}_NAME} WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
	endif ()
ENDMACRO( STRIP )

#---------------------------------------------------------------------------

MACRO( ROOT X )
	if (NOT CMAKE_CROSSCOMPILING)
		add_custom_command( TARGET ${${X}_NAME} POST_BUILD COMMAND echo user | sudo -S chown root:root ${${X}_NAME} 2>/dev/null WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
		add_custom_command( TARGET ${${X}_NAME} POST_BUILD COMMAND echo user | sudo -S chmod 6755 ${${X}_NAME} 2>/dev/null WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
	endif ()
ENDMACRO( ROOT )

#---------------------------------------------------------------------------

MACRO( DEPEND X )
	add_dependencies( ${${X}_NAME} ${ARGN} )
ENDMACRO( DEPEND )

#---------------------------------------------------------------------------

