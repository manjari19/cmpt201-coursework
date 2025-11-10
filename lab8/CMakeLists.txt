cmake_minimum_required(VERSION 3.22)

project(
  Lab8
  VERSION 1.0
  DESCRIPTION "This is an example for the lab."
  LANGUAGES C)

set(CMAKE_BUILD_TYPE Debug)

# Note: ASan and TSan are incompatible
# Address sanitization:
# add_compile_options(-fsanitize=address) 
# add_link_options(-fsanitize=address)
# --------- Task 3 ---------
# Enable the thread sanitizer 
add_compile_options(-fsanitize=thread -g -O1)
add_link_options(-fsanitize=thread -g -O1)

# Create the executable from src/lab8.c
add_executable(lab8 src/lab8.c)

# Include the local "include" directory so uthash.h is found
target_include_directories(lab8 PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link pthreads
find_package(Threads REQUIRED)
target_link_libraries(lab8 PRIVATE Threads::Threads)

add_compile_options(-pthread)
add_link_options(-pthread)

add_executable(main src/lab8.c)
target_include_directories(main PRIVATE include)

