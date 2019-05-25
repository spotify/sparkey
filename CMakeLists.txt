project (sparkey)
cmake_minimum_required (VERSION 2.7)

add_subdirectory (src)

find_package (Doxygen)
if (DOXYGEN_FOUND)
	add_custom_target (doc
		${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Running Doxygen" VERBATIM)
endif ()
