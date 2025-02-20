cmake_minimum_required(VERSION 3.31.5)
project(wart VERSION 3.31.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# force clang
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -march=native -mtune=native -DNDEBUG -lcurl -O2")
set(CMAKE_CXX_FLAGS_DEBUG "-O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -lcurl -O2")

# exec
add_executable(wart wart.cc)
target_link_libraries(wart ${CURL_LIBRARIES})

# use llvm toolchain optimizations
target_link_options(wart PRIVATE -flto -Wl,--strip-all -Wl,--gc-sections)

# enable warnings and security flags
target_compile_options(wart PRIVATE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wnull-dereference -Wdouble-promotion -Wformat=2
  -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC
  -ffunction-sections -fdata-sections
)

# enable sanitizers in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(wart PRIVATE -fsanitize=address,undefined)
  target_link_options(wart PRIVATE -fsanitize=address,undefined)
endif()
