cmake_minimum_required(VERSION 3.14)
project(SimpleXmlJsonConverter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)

add_library(SimpleXmlJsonConverter STATIC
  SimpleXmlJsonConverter.cpp
  SimpleXmlJsonConverter.hpp
)
target_link_libraries(SimpleXmlJsonConverter PUBLIC pugixml)
target_include_directories(SimpleXmlJsonConverter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(SimpleXmlJsonConverterExe main.cpp)
target_link_libraries(SimpleXmlJsonConverterExe PUBLIC SimpleXmlJsonConverter stdc++fs)

install(TARGETS SimpleXmlJsonConverterExe RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}")
