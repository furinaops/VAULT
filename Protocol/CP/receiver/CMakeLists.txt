add_executable(cp_receiver
    main.cpp
    cp_receiver_node.cpp
)
target_link_libraries(cp_receiver
    cp_shared
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_include_directories(cp_receiver PRIVATE ../shared)
