#include <iostream>
#include "cp_config.hpp"
#include "cp_receiver_node.hpp"
#include "../shared/cp_crypto.hpp"

int main() {
    CPConfig config = CPConfig::loadFromFile("factory.cpproj");

    std::cout << "CP Protocol — Receiver Node\nEnter server login key: ";
    std::string hex;
    std::cin >> hex;

    std::array<uint8_t, 32> key;
    try {
        key = CPCrypto::hexToBytes(hex);
    } catch (...) {
        std::cerr << "Invalid key format.\n";
        return 1;
    }

    CPReceiverNode node(config, key);
    node.connect();

    // Register all triggers defined in the config for this agent
    const AgentConfig* my_agent = nullptr;
    for (const auto& a : config.agents) {
        if (a.role == "responder") {
            my_agent = &a;
            break;
        }
    }

    if (my_agent) {
        for (const auto& trigger : my_agent->triggers) {
            node.registerTrigger(trigger, [trigger](const std::string& params) {
                std::cout << "\n[Incoming Message]" << std::endl;
                std::cout << "Trigger: " << trigger << std::endl;
                std::cout << "Message: " << params << std::endl;
                std::cout << "------------------" << std::endl;
            });
        }
    }

    std::cout << "Receiver ready. Listening for commands..." << std::endl;
    node.run();
    return 0;
}
