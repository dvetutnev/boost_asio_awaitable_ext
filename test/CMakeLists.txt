add_executable(tests
    test_timer.cpp
    test_schedule.cpp
    test_event.cpp)

target_link_libraries(tests
    ${PROJECT_NAME}
    GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(tests)
