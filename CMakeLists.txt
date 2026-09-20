cmake_minimum_required(VERSION 3.16)
project(reactornet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Werror)
add_compile_definitions(_GNU_SOURCE)

# ---- 库（源文件随任务逐步追加）----
add_library(reactornet
  src/EventLoop.cpp
  src/Channel.cpp
  src/Acceptor.cpp
  src/TcpConnection.cpp
  src/TcpServer.cpp
)
target_include_directories(reactornet PUBLIC include)

# ---- 测试 ----
enable_testing()
add_executable(test_channel test/test_channel.cpp)
target_link_libraries(test_channel PRIVATE reactornet)
add_test(NAME channel COMMAND test_channel)
add_executable(test_acceptor test/test_acceptor.cpp)
target_link_libraries(test_acceptor PRIVATE reactornet)
add_test(NAME acceptor COMMAND test_acceptor)
add_executable(test_tcpconn test/test_tcpconn.cpp)
target_link_libraries(test_tcpconn PRIVATE reactornet)
add_test(NAME tcpconn COMMAND test_tcpconn)
add_executable(test_echo test/test_echo.cpp)
target_link_libraries(test_echo PRIVATE reactornet)
add_test(NAME echo COMMAND test_echo)

# ---- 示例 ----
add_executable(echo_server example/echo_server.cpp)
target_link_libraries(echo_server PRIVATE reactornet)
