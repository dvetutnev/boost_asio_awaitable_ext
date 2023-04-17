add_executable(
  tsan_tests sequence_barrier.cpp sequence_barrier_mock_awaiter.cpp
             multi_producer_sequencer.cpp ../main.cpp)
target_link_libraries(tsan_tests PRIVATE ${PROJECT_NAME} disruptorplus
                                         Boost::unit_test_framework)
target_compile_options(tsan_tests PRIVATE "-fsanitize=thread")
target_link_options(tsan_tests PRIVATE "-fsanitize=thread")
add_test(NAME tsan_tests COMMAND tsan_tests)
