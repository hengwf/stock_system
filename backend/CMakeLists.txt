cmake_minimum_required(VERSION 3.10)
project(quant_server CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /O2")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
else()
    add_compile_options(-Wall -Wextra)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

set(SOURCES
    src/main.cpp
    src/common/stock_data.cpp
    src/common/risk_control.cpp
    src/datasource/sina_provider.cpp
    src/selector/stock_selector.cpp
    src/selector/quant_selector.cpp
    src/selector/long_term.cpp
    src/selector/mid_term.cpp
    src/selector/short_term.cpp
    src/selector/ultra_short_term.cpp
    src/cache/data_cache.cpp
    src/server/http_server.cpp
    src/server/api_handler.cpp
)

add_executable(quant-server ${SOURCES})

if(WIN32)
    target_link_libraries(quant-server ws2_32 wininet)
endif()
