add_executable(
  tests
  test_timer.cpp
  test_schedule.cpp
  test_event.cpp
  test_sequence_traits.cpp
  test_sequence_barrier.cpp
  test_sequence_barrier_group.cpp
  test_single_producer_sequencer.cpp
  test_multi_producer_sequencer.cpp
  test_asio_multithread.cpp
  sequence_barrier_mock_awaiter.cpp
  main.cpp)
target_link_libraries(tests PRIVATE ${PROJECT_NAME} Boost::unit_test_framework)
add_test(NAME ${PROJECT_NAME} COMMAND tests)

if(TSAN_TESTS)
  add_executable(tsan_tests main.cpp tsan/multi_producer_sequencer.cpp)
  target_link_libraries(tsan_tests PRIVATE ${PROJECT_NAME} disruptorplus
                                           Boost::unit_test_framework)
  target_compile_options(tsan_tests PRIVATE "-fsanitize=thread")
  target_link_options(tsan_tests PRIVATE "-fsanitize=thread")
  add_test(NAME tsan_tests COMMAND tsan_tests)
endif()
