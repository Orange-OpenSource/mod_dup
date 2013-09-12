cmake_minimum_required(VERSION 2.8)

find_package(Boost REQUIRED COMPONENTS thread regex)
find_package(APR REQUIRED)
find_package(CURL REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${APR_INCLUDE_DIR})
include_directories(/usr/include/apache2)
include_directories(${CURL_INCLUDE_DIR})
