add_executable(tests
    test_timer.cpp
    test_schedule.cpp
    test_event.cpp
    test_sequence_traits.cpp
    test_sequence_barrier.cpp
    test_asio_multithread.cpp
    main.cpp)

target_link_libraries(tests
    ${PROJECT_NAME}
    Boost::test
    boost_unit_test_framework)

add_test(NAME ${PROJECT_NAME} COMMAND tests)
