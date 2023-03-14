add_executable(tests
    test_timer.cpp
    test_schedule.cpp
    test_event.cpp
    test_sequence_traits.cpp
    test_sequence_barrier.cpp
    test_single_producer_sequencer.cpp
    test_multi_producer_sequencer.cpp
    test_asio_multithread.cpp
    sequence_barrier_mock_awaiter.cpp
    main.cpp)

target_link_libraries(tests
    ${PROJECT_NAME}
    Boost::test
    boost_unit_test_framework)

add_test(NAME ${PROJECT_NAME} COMMAND tests)
