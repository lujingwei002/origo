cmake_minimum_required (VERSION 2.8)
project (gate)

#项目选项
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set (LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#编译选项
add_compile_options (-std=c++11 -Wall -O2 -g) 

#头文件
include_directories (src)
include_directories (include)
include_directories (dep/ae)

#源文件
aux_source_directory (src SRC_LIST)
aux_source_directory (src/client SRC_LIST)
aux_source_directory (src/encrypt SRC_LIST)
aux_source_directory (src/upstream SRC_LIST)

#库文件
find_library(HTTP_PARSER_LIB http_parser HINTS ${PROJECT_SOURCE_DIR}/lib/)
find_library(JSONCPP_LIB jsoncpp HINTS ${PROJECT_SOURCE_DIR}/lib/)
find_library(HIREDIS_LIB hiredis HINTS ${PROJECT_SOURCE_DIR}/lib/)

#gate
add_executable (gate main.cc dep/ae/ae.cc  ${SRC_LIST})
target_link_libraries (gate ${HTTP_PARSER_LIB})
target_link_libraries (gate ${JSONCPP_LIB})
target_link_libraries (gate ${HIREDIS_LIB})
target_link_libraries (gate ssl)
target_link_libraries (gate crypto)
target_link_libraries (gate dl)
target_link_libraries (gate m)
target_link_libraries (gate ncurses)
target_link_libraries (gate readline)
target_link_libraries (gate pthread)

