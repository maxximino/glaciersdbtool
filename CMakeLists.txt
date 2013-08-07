cmake_minimum_required(VERSION 2.8)
project(glaciersdbtool)
add_executable(glaciersdbtool metadata.cpp awsrequest.cpp config.cpp uploadjob.cpp cryptoutils.cpp main.cpp)
find_package(Boost COMPONENTS iostreams REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
include_directories(
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
)
target_link_libraries(glaciersdbtool
  ${Boost_IOSTREAMS_LIBRARY}
  ${OPENSSL_LIBRARIES}
  ${CURL_LIBRARY}
)
add_definitions("-std=c++0x")

