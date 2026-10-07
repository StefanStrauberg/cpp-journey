cmake_minimum_required(VERSION 3.20)
project(cpp_journey CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Создаем таргеты
#add_executable(HelloWorld 01_HelloWorld/main.cpp)
#add_executable(NetCodeJourn 02_NetCodeJourn/main.cpp)
add_executable(game src/main.cpp)

# Настраиваем предупреждения (безопасно для всех ОС и компиляторов)
if(MSVC)
    #target_compile_options(HelloWorld PRIVATE /W4)
    #target_compile_options(NetCodeJourn PRIVATE /W4)
    target_compile_options(game PRIVATE /W4)
else()
    set(MY_GCC_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    #target_compile_options(HelloWorld PRIVATE ${MY_GCC_FLAGS})
    #target_compile_options(NetCodeJourn PRIVATE ${MY_GCC_FLAGS})
    target_compile_options(game PRIVATE ${MY_GCC_FLAGS})
endif()
