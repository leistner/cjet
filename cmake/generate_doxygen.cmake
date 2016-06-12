FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
    SET(CJET_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Doxyfile)
    ADD_CUSTOM_TARGET(doc ALL ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/generated/Doxyfile WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMENT "Generating API documentation with Doxygen" VERBATIM)
ENDIF(DOXYGEN_FOUND)
