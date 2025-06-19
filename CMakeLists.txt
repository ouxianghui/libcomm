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
        rtc_base/win32.cc
        rtc_base/task_queue_win.cc
    )
    set(PLATFORM_HEADERS
        rtc_base/byte_order.h
        rtc_base/task_queue_win.h
        rtc_base/win32.h
    )
elseif(APPLE)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/absl/mac/include)
    set(PLATFORM_SOURCES
        api/task_queue/default_task_queue_factory_gcd.cc
        rtc_base/task_queue_gcd.cc
        rtc_base/system/gcd_helpers.mm
        rtc_base/system/cocoa_threading.mm
    )
    set(PLATFORM_HEADERS
        rtc_base/synchronization/mutex_pthread.h
        rtc_base/task_queue_gcd.h
        rtc_base/system/gcd_helpers.h
        rtc_base/system/cocoa_threading.h
    )
else()
    set(PLATFORM_SOURCES
        api/task_queue/default_task_queue_factory_stdlib.cc
    )
    set(PLATFORM_HEADERS "") # Linux aarch64 has no specific headers in this list
endif()

set(HEADERS
    json_serializer.hpp
    proxy.hpp
    main_thread.hpp
    signal.hpp
    observable.hpp
    light_weight_semaphore.hpp
    api/sequence_checker.h
    api/units/time_delta.h
    api/units/timestamp.h
    api/task_queue/task_queue_base.h
    api/task_queue/task_queue_factory.h
    api/task_queue/pending_task_safety_flag.h
    rtc_base/buffer_queue.h
    rtc_base/byte_buffer.h
    rtc_base/buffer.h
    rtc_base/byte_order.h
    rtc_base/callback_list.h
    rtc_base/crc32.h
    rtc_base/cpu_info.h
    rtc_base/cpu_time.h
    rtc_base/weak_ptr.h
    rtc_base/string_utils.h
    rtc_base/file_rotating_stream.h
    rtc_base/log_sinks.h
    rtc_base/checks.h
    rtc_base/event.h
    rtc_base/platform_thread.h
    rtc_base/platform_thread_types.h
    rtc_base/thread.h
    rtc_base/time_utils.h
    rtc_base/system_time.h
    rtc_base/logging.h
    rtc_base/null_socket_server.h
    rtc_base/memory_stream.h
    rtc_base/memory_usage.h
    rtc_base/one_time_event.h
    rtc_base/operations_chain.h
    rtc_base/race_checker.h
    rtc_base/rate_statistics.h
    rtc_base/rate_limiter.h
    rtc_base/rate_tracker.h
    rtc_base/random.h
    rtc_base/frequency_tracker.h
    rtc_base/stream.h
    rtc_base/strong_alias.h
    rtc_base/containers/flat_map.h
    rtc_base/containers/flat_set.h
    rtc_base/containers/flat_tree.h
    rtc_base/containers/invoke.h
    rtc_base/memory/aligned_malloc.cc
    rtc_base/memory/fifo_buffer.cc
    rtc_base/synchronization/yield_policy.h
    rtc_base/synchronization/yield.h
    rtc_base/synchronization/sequence_checker_internal.h
    rtc_base/strings/string_builder.h
    rtc_base/system/file_wrapper.h
    rtc_base/task_utils/repeating_task.h
    rtc_base/third_party/sigslot/sigslot.h
    rtc_base/internal/default_socket_server.h
    system_wrappers/include/clock.h
    system_wrappers/include/field_trial.h
    system_wrappers/include/metrics.h
    system_wrappers/include/ntp_time.h
    ${PLATFORM_HEADERS}
)

set(SOURCES
    proxy.cpp
    main_thread.cpp
    api/rtc_error.cc
    api/units/time_delta.cc
    api/units/timestamp.cc
    api/task_queue/task_queue_base.cc
    api/task_queue/pending_task_safety_flag.cc
    rtc_base/buffer_queue.cc
    rtc_base/byte_buffer.cc
    rtc_base/callback_list.cc
    rtc_base/crc32.cc
    rtc_base/cpu_info.cc
    rtc_base/cpu_time.cc
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
    rtc_base/memory_stream.cc
    rtc_base/memory_usage.cc
    rtc_base/operations_chain.cc
    rtc_base/race_checker.cc
    rtc_base/rate_statistics.cc
    rtc_base/rate_limiter.cc
    rtc_base/rate_tracker.cc
    rtc_base/random.cc
    rtc_base/frequency_tracker.cc
    rtc_base/stream.cc
    rtc_base/containers/flat_tree.cc
    rtc_base/synchronization/yield_policy.cc
    rtc_base/synchronization/yield.cc
    rtc_base/synchronization/sequence_checker_internal.cc
    rtc_base/strings/string_builder.cc
    rtc_base/strings/string_format.cc
    rtc_base/system/file_wrapper.cc
    rtc_base/task_utils/repeating_task.cc
    rtc_base/third_party/sigslot/sigslot.cc
    rtc_base/internal/default_socket_server.cc
    system_wrappers/source/clock.cc
    system_wrappers/source/field_trial.cc
    system_wrappers/source/metrics.cc
    ${PLATFORM_SOURCES}
)

# --- 创建主可执行文件 ---
add_executable(libcomm)

# --- 为目标添加源文件和头文件 ---
# 这样可以确保IDE能看到所有文件，同时只编译源文件
target_sources(libcomm PRIVATE
    main.cpp
    ${SOURCES}
    ${HEADERS}
)


if(APPLE)
    target_link_libraries(libcomm PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreFoundation"
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_strings.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_strings_internal.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_string_view.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_base.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_raw_logging_internal.a
    )
elseif(WIN32)
    target_link_libraries(libcomm PRIVATE
        Ws2_32.lib
        Winmm.lib
        ${CMAKE_CURRENT_LIST_DIR}/absl/win/lib/abseil_dll.lib
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

# ===================================================================
#  测试配置
# ===================================================================
enable_testing()

# 在 include(FetchContent) 之前设置策略以消除警告
cmake_policy(SET CMP0135 NEW)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(run_tests
    json_serializer_test.cpp
    rtc_base/callback_list_unittest.cc
    rtc_base/event_unittest.cc
    rtc_base/byte_order_unittest.cc
    # rtc_base/file_rotating_stream_unittest.cc
    rtc_base/logging_unittest.cc
    # rtc_base/thread_unittest.cc
    rtc_base/strong_alias_unittest.cc
    rtc_base/cpu_time_unittest.cc
    rtc_base/crc32_unittest.cc
    # rtc_base/operations_chain_unittest.cc
    rtc_base/one_time_event_unittest.cc
    rtc_base/buffer_queue_unittest.cc
    rtc_base/byte_buffer_unittest.cc
    rtc_base/byte_order_unittest.cc
    rtc_base/frequency_tracker_unittest.cc
    rtc_base/random_unittest.cc
    rtc_base/rate_limiter_unittest.cc
    rtc_base/rate_statistics_unittest.cc
    rtc_base/rate_tracker_unittest.cc
    ${SOURCES}
)

target_link_libraries(run_tests PRIVATE GTest::gmock GTest::gtest_main)

if(APPLE)
    target_link_libraries(run_tests PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreFoundation"
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_strings.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_base.a
        ${CMAKE_CURRENT_LIST_DIR}/absl/mac/lib/libabsl_raw_logging_internal.a
    )
elseif(WIN32)
    target_link_libraries(libcomm PRIVATE
        Ws2_32.lib
        Winmm.lib
        ${CMAKE_CURRENT_LIST_DIR}/absl/win/lib/abseil_dll.lib
    )
else()
    target_link_libraries(libcomm PRIVATE
    )
endif()

target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/rapidjson
)
if(APPLE)
    target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/absl/mac/include)
endif()

include(GoogleTest)
gtest_discover_tests(run_tests)
