#pragma once
#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/delta.hpp"
#include "vault/name_registry.hpp"
#include "vault/author_registry.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <cstring>
#include <algorithm>

struct VaultReceiver {
    ObjectStore store;
    VersionChain chain;
    NameRegistry names;
    AuthorRegistry authors;
    std::string storage_path;
    std::string owner_key;
    std::string guest_key;
    size_t max_repo_size_mb;

    VaultReceiver(const std::string& sp,
                  const std::string& ok,
                  const std::string& gk,
                  size_t max_mb);
    std::string handleHookInit(const std::string& params);
    std::string handleHookData(const std::string& params);
    std::string handleGetReq(const std::string& params);
    std::string handleGetAck(const std::string& params);
    std::string handleSicQuery(const std::string& params);
    std::string handleSyncReq(const std::string& params);
    std::string handleVerifyReq(const std::string& params);
    std::string handleRollbackReq(const std::string& params);
};
