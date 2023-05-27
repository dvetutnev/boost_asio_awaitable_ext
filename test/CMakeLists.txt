add_executable(
  tests
  test_examples.cpp
  test_schedule.cpp
  test_event.cpp
  test_sequence_traits.cpp
  test_sequence_barrier.cpp
  test_sequence_barrier_group.cpp
  test_single_producer_sequencer.cpp
  test_multi_producer_sequencer.cpp
  test_asio_multithread.cpp
  test_nats_coro.cpp
  test_concepts.cpp
  test_queue.cpp
  main.cpp)
target_link_libraries(tests PRIVATE ${PROJECT_NAME} Boost::unit_test_framework)
add_test(NAME ${PROJECT_NAME} COMMAND tests)

if(TSAN_TESTS)
  add_subdirectory(tsan)
endif()
