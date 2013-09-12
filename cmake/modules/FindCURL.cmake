# Defines
# CURL_FOUND
# CURL_INCLUDE_DIR
# CURL_LIBRARIES

OPTION(CURL_IS_STATIC "on if curl is a static lib " OFF)

set(CURL_FIND_DEBUG TRUE CACHE BOOL "Also search for the debug version of the QuikGrid library")

if(CURL_FIND_DEBUG)
	if(CURL_INCLUDE_DIR AND CURL_LIBRARY AND CURL_LIBRARY_DEBUG)
		set(CURL_FIND_QUIETLY TRUE)
	endif(CURL_INCLUDE_DIR AND CURL_LIBRARY AND CURL_LIBRARY_DEBUG)
else(CURL_FIND_DEBUG)
	if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
		set(CURL_FIND_QUIETLY TRUE)
	endif(CURL_INCLUDE_DIR AND CURL_LIBRARY)
endif(CURL_FIND_DEBUG)

find_path(CURL_INCLUDE_DIR NAMES curl/curl.h DOC "Directory containing curl/curl.h")

find_library(CURL_LIBRARY NAMES curl DOC "Path to curl library")
if(CURL_FIND_DEBUG)
	find_library(CURL_LIBRARY_DEBUG NAMES curld DOC "Path to curl debug library")
endif(CURL_FIND_DEBUG)

# handle the QUIETLY and REQUIRED arguments and set CURL_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CURL DEFAULT_MSG CURL_LIBRARY CURL_INCLUDE_DIR)

if(CURL_FOUND)
	if(CURL_FIND_DEBUG)
		if(NOT CURL_LIBRARY_DEBUG)
			set(CURL_LIBRARY_DEBUG ${CURL_LIBRARY})
		endif(NOT CURL_LIBRARY_DEBUG)
		if(WIN32 AND CURL_IS_STATIC)
			set(CURL_LIBRARIES debug ${CURL_LIBRARY_DEBUG} optimized ${CURL_LIBRARY} general ws2_32.lib winmm.lib)
		else(WIN32 AND CURL_IS_STATIC)
			set(CURL_LIBRARIES debug ${CURL_LIBRARY_DEBUG} optimized ${CURL_LIBRARY})
		endif(WIN32 AND CURL_IS_STATIC)
	else(CURL_FIND_DEBUG)
		if(WIN32 AND CURL_IS_STATIC)
			set(CURL_LIBRARIES ${CURL_LIBRARY} ws2_32.lib winmm.lib)
		else(WIN32 AND CURL_IS_STATIC)
			set(CURL_LIBRARIES ${CURL_LIBRARY})
		endif(WIN32 AND CURL_IS_STATIC)
	endif(CURL_FIND_DEBUG)
else(CURL_FOUND)
	SET(CURL_LIBRARIES )
endif(CURL_FOUND)
