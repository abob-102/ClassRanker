if(NOT DEFINED OPENXLSX_SOURCE_DIR)
    message(FATAL_ERROR "OPENXLSX_SOURCE_DIR is required")
endif()

set(header "${OPENXLSX_SOURCE_DIR}/OpenXLSX/headers/XLSheet.hpp")
file(READ "${header}" content)
string(REPLACE
    "query.template setParam(\"sheetID\", relationshipID());"
    "query.setParam(\"sheetID\", relationshipID());"
    content "${content}")
file(WRITE "${header}" "${content}")

