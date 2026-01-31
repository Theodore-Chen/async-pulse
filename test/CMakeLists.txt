cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_SANITIZERS "Enable sanitizers for testing (address, undefined, thread, etc.)" OFF)

if(ENABLE_SANITIZERS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Sanitizers are best used with Debug builds. Forcing CMAKE_BUILD_TYPE to Debug.")
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()

    if(ENABLE_SANITIZERS STREQUAL "address")
        message(STATUS "AddressSanitizer enabled for tests")
        set(SANITIZER_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g")
    elseif(ENABLE_SANITIZERS STREQUAL "undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled for tests")
        set(SANITIZER_FLAGS "-fsanitize=undefined -fno-omit-frame-pointer -g")
    elseif(ENABLE_SANITIZERS STREQUAL "thread")
        message(STATUS "ThreadSanitizer enabled for tests")
        set(SANITIZER_FLAGS "-fsanitize=thread -fno-omit-frame-pointer -g")
    else()
        message(STATUS "Multiple sanitizers enabled for tests: ${ENABLE_SANITIZERS}")
        set(SANITIZER_FLAGS "-fsanitize=${ENABLE_SANITIZERS} -fno-omit-frame-pointer -g")
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

find_package(GTest REQUIRED)

include(GoogleTest)

add_subdirectory(unit)

add_subdirectory(stress)
