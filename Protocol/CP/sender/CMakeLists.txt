add_executable(cp_sender
    main.cpp
    cp_server.cpp
    cp_sender_node.cpp
)
target_link_libraries(cp_sender
    cp_shared
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_include_directories(cp_sender PRIVATE ../shared)
