cmake_minimum_required(VERSION 3.16)

project(libcomm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DWEBRTC_WIN -DNOMINMAX)
elseif(APPLE)
    add_definitions(-DWEBRTC_POSIX -DWEBRTC_MAC)
else()
    add_definitions(-DWEBRTC_POSIX -DWEBRTC_LINUX)
endif()

add_definitions(-D__native_client__)

include_directories(${CMAKE_CURRENT_LIST_DIR})


if(WIN32)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/absl/win/include)
    set(PLATFORM_SOURCES
        api/task_queue/default_task_queue_factory_win.cc
        rtc_base/byte_order.h
        rtc_base/win32.h
        rtc_base/win32.cc
        rtc_base/task_queue_win.cc
        rtc_base/task_queue_win.h
    )
elseif(APPLE)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/absl/mac/include)
    set(PLATFORM_SOURCES
        api/task_queue/default_task_queue_factory_gcd.cc
        rtc_base/synchronization/mutex_pthread.h
        rtc_base/task_queue_gcd.cc
        rtc_base/task_queue_gcd.h
        rtc_base/system/gcd_helpers.h
        rtc_base/system/gcd_helpers.mm
        rtc_base/system/cocoa_threading.h
        rtc_base/system/cocoa_threading.mm
    )
else()
    set(PLATFORM_SOURCES
        api/task_queue/default_task_queue_factory_stdlib.cc
    )
endif()

set(SOURCES
    main.cpp
    proxy.cpp
    main_thread.cpp
    api/units/time_delta.cc
    api/units/timestamp.cc
    api/task_queue/task_queue_base.cc
    rtc_base/weak_ptr.cc
    rtc_base/string_utils.cc
    rtc_base/string_encode.cc
    rtc_base/string_to_number.cc
    rtc_base/file_rotating_stream.cc
    rtc_base/log_sinks.cc
    rtc_base/checks.cc
    rtc_base/event.cc
    rtc_base/platform_thread.cc
    rtc_base/platform_thread_types.cc
    rtc_base/thread.cc
    rtc_base/time_utils.cc
    rtc_base/system_time.cc
    rtc_base/logging.cc
    rtc_base/null_socket_server.cc
    rtc_base/synchronization/yield_policy.cc
    rtc_base/synchronization/yield.cc
    rtc_base/synchronization/sequence_checker_internal.cc
    rtc_base/strings/string_builder.cc
    rtc_base/strings/string_format.cc
    rtc_base/system/file_wrapper.cc
    rtc_base/internal/default_socket_server.cc
    ${PLATFORM_SOURCES}
)

add_executable(libcomm ${SOURCES})

if(APPLE)
    set_source_files_properties(main.cpp PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

if(WIN32)
    target_link_libraries(libcomm PRIVATE
        Ws2_32.lib
        Winmm.lib
        ${CMAKE_CURRENT_LIST_DIR}/absl/win/lib/abseil_dll.lib
    )
elseif(APPLE)
    target_link_libraries(libcomm PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreFoundation"
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_strings.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_base.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_raw_logging_internal.a
    )
else()
    target_link_libraries(libcomm PRIVATE
    )
endif()

include(GNUInstallDirs)
install(TARGETS libcomm
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
