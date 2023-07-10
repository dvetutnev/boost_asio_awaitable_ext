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
  test_connect_to_nats.cpp
  test_message.cpp
  test_subscribe.cpp
  test_received.cpp
  test_parallel_group.cpp
  utils.cpp
  main.cpp)
target_link_libraries(tests PRIVATE ${PROJECT_NAME} Boost::unit_test_framework)
add_test(NAME ${PROJECT_NAME} COMMAND tests)

if(TSAN_TESTS)
  add_subdirectory(tsan)
endif()

add_executable(example_service example_service.cpp)
target_link_libraries(example_service ${PROJECT_NAME} OpenSSL::Crypto
                      OpenSSL::SSL)

add_executable(wait_signal wait_signal.cpp)
target_link_libraries(example_service Boost::headers)
