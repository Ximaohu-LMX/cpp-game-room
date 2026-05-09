find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found, skip tests")
    return()
endif()

add_executable(game_tests
    test_room_state.cpp
    test_match_queue.cpp
    test_protocol.cpp
    test_settlement.cpp
)

target_link_libraries(game_tests PRIVATE game_core protobuf::libprotobuf GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(game_tests)
