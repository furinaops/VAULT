#include <iostream>
#include <fstream>
#include <thread>
#include "cp_config.hpp"
#include "cp_server.hpp"
#include "cp_sender_node.hpp"
#include "../shared/cp_crypto.hpp"

int main() {
    CPConfig config = CPConfig::loadFromFile("factory.cpproj");
    std::cout << "Config loaded." << std::endl;

    const std::string key_path = "sender/.cp_login_key";
    std::array<uint8_t, 32> login_key;
    std::ifstream key_file(key_path);
    if (key_file.good()) {
        std::string hex;
        std::getline(key_file, hex);
        login_key = CPCrypto::hexToBytes(hex);
        std::cout << "Login key loaded." << std::endl;
    } else {
        login_key = CPCrypto::generateLoginKey();
        std::string hex = CPCrypto::bytesToHex(login_key);
        std::ofstream out(key_path);
        out << hex;
        std::cout << R"(
╔════════════════════════════════════╗
║  CP Server — First Time Setup      ║
║  Login Key:                        ║
║  )" << hex << R"(   ║
║  Share this with receiver nodes    ║
╚════════════════════════════════════╝
)" << std::endl;
    }

    CPServer server(login_key, config);
    server.start();
    std::cout << "Server running on port " << config.server_port << std::endl;
    std::cout << "Waiting for receiver nodes to connect..." << std::endl;

    CPSenderNode node(config, login_key);
    server.waitForAllReceivers();

    node.connect();

    std::cout << "\n" << R"(
╔══════════════════════════════════════════════════════╗
║             CP PROTOCOL CHAT CONSOLE                 ║
║                                                      ║
║   Usage: <TRIGGER> <MESSAGE>                         ║
║   Example: MOVE Forward 10 meters                    ║
║   Type 'exit' to close session                       ║
╚══════════════════════════════════════════════════════╝
)" << std::endl;

    std::string line;
    while (true) {
        std::cout << "Sender > " << std::flush;
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line.empty()) continue;

        size_t space = line.find(' ');
        std::string trigger = line.substr(0, space);
        std::string params = (space == std::string::npos) ? "" : line.substr(space + 1);

        node.send(DialogueRole::PROPOSE, trigger, params);
    }

    node.disconnect();
    server.stop();
    return 0;
}
