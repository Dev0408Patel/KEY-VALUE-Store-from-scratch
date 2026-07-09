# Configure the test executable target
add_executable(fastkv_tests
    kv_store_test.cpp
)

# Link against the core library and GoogleTest
target_link_libraries(fastkv_tests
    PRIVATE
        fastkv
        gtest_main
)

target_compile_options(fastkv_tests PRIVATE ${FASTKV_WARNINGS})

# Register the target executable with CTest framework
add_test(NAME FastKVTests COMMAND fastkv_tests)
