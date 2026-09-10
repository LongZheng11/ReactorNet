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
  )
  target_include_directories(reactornet PUBLIC include)

  # ---- 测试 ----
  enable_testing()
  add_executable(test_channel test/test_channel.cpp)
  target_link_libraries(test_channel PRIVATE reactornet)
  add_test(NAME channel COMMAND test_channel)