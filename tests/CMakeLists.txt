enable_testing()

# Include CMocka headers globally for tests
include_directories(${cmocka_SOURCE_DIR}/include)
include_directories(${cmocka_BINARY_DIR}/include)

# Helper function to add a unit test
function(add_unit_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE cmocka)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_subdirectory(unit)
