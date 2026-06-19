add_library(cp_shared STATIC
    cp_packet.cpp
    cp_session.cpp
    cp_crypto.cpp
    cp_config.cpp
)
target_include_directories(cp_shared PUBLIC .)
target_link_libraries(cp_shared OpenSSL::Crypto)
