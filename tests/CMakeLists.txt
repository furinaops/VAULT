find_package(OpenSSL REQUIRED)
find_package(GTest QUIET)

if(GTest_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})

    add_executable(test_sic test_sic.cpp)
    target_link_libraries(test_sic vault_core ${GTEST_LIBRARIES} pthread)
    add_test(NAME SICTests COMMAND test_sic)

    add_executable(test_object_store test_object_store.cpp)
    target_link_libraries(test_object_store vault_core ${GTEST_LIBRARIES} pthread)
    add_test(NAME ObjectStoreTests COMMAND test_object_store)

    add_executable(test_version_chain test_version_chain.cpp)
    target_link_libraries(test_version_chain vault_core ${GTEST_LIBRARIES} pthread)
    add_test(NAME VersionChainTests COMMAND test_version_chain)

    add_executable(test_delta test_delta.cpp)
    target_link_libraries(test_delta vault_core ${GTEST_LIBRARIES} pthread)
    add_test(NAME DeltaTests COMMAND test_delta)
else()
    add_executable(test_sic test_sic.cpp)
    target_link_libraries(test_sic vault_core pthread)
    add_custom_target(run_tests
        COMMAND test_sic
        COMMAND test_object_store
        COMMAND test_version_chain
        COMMAND test_delta
    )
endif()
