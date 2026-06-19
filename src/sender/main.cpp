#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

int cmdHook(int argc, char* argv[]);
int cmdGet(int argc, char* argv[]);
int cmdSic(int argc, char* argv[]);
int cmdDiff(int argc, char* argv[]);
int cmdSync(int argc, char* argv[]);
int cmdRollback(int argc, char* argv[]);
int cmdVerify(int argc, char* argv[]);

static void ensureVaultDir() {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    fs::create_directories(home + "/.vault/data/objects");
    fs::create_directories(home + "/.vault/data/versions");
    if (!fs::exists(home + "/.vault/author.json")) {
        std::string default_author =
            "{\n  \"default\": {\n"
            "    \"id\": \"default\",\n"
            "    \"display_name\": \"Default User\",\n"
            "    \"public_key\": \"\"\n"
            "  }\n}\n";
        std::ofstream f(home + "/.vault/author.json");
        f << default_author;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "VAULT — Personal Cryptographic Version Control\n\n"
                  << "Commands:\n"
                  << "  /HOOK \"<path>\" --n \"<name>\" [--msg \"<desc>\"]\n"
                  << "  /GET \"<sic_or_name>\" [--delta] [--log]\n"
                  << "  /SIC \"<name>\" [--log] [--log --n <count>]\n"
                  << "  /DIFF \"<a>\" \"<b>\"\n"
                  << "  /SYNC\n"
                  << "  /ROLLBACK \"<sic_or_name>\"\n"
                  << "  /VERIFY [<sic>]\n";
        return 0;
    }
    ensureVaultDir();
    std::string cmd = argv[1];
    int cmd_argc = argc - 1;
    char** cmd_argv = argv + 1;
    if (cmd == "/HOOK" || cmd == "hook" || cmd == "HOOK") {
        return cmdHook(cmd_argc, cmd_argv);
    } else if (cmd == "/GET" || cmd == "get" || cmd == "GET") {
        return cmdGet(cmd_argc, cmd_argv);
    } else if (cmd == "/SIC" || cmd == "sic" || cmd == "SIC") {
        return cmdSic(cmd_argc, cmd_argv);
    } else if (cmd == "/DIFF" || cmd == "diff" || cmd == "DIFF") {
        return cmdDiff(cmd_argc, cmd_argv);
    } else if (cmd == "/SYNC" || cmd == "sync" || cmd == "SYNC") {
        return cmdSync(cmd_argc, cmd_argv);
    } else if (cmd == "/ROLLBACK" || cmd == "rollback" || cmd == "ROLLBACK") {
        return cmdRollback(cmd_argc, cmd_argv);
    } else if (cmd == "/VERIFY" || cmd == "verify" || cmd == "VERIFY") {
        return cmdVerify(cmd_argc, cmd_argv);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        std::cerr << "Run without arguments for usage.\n";
        return 1;
    }
}
