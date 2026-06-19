#include "receiver.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>

VaultReceiver::VaultReceiver(const std::string& sp,
                              const std::string& ok,
                              const std::string& gk,
                              size_t max_mb)
    : store(sp + "/data")
    , chain(store)
    , names(sp + "/data")
    , authors()
    , storage_path(sp)
    , owner_key(ok)
    , guest_key(gk)
    , max_repo_size_mb(max_mb)
{}

std::string VaultReceiver::handleHookInit(const std::string& params) {
    std::istringstream ss(params);
    std::string token, name, message, parent_sic;
    int file_count = 0;
    size_t total_size = 0;
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("session_token:") == 0)
            token = line.substr(std::string("session_token:").size());
        else if (line.find("name:") == 0)
            name = line.substr(std::string("name:").size());
        else if (line.find("message:") == 0)
            message = line.substr(std::string("message:").size());
        else if (line.find("parent_sic:") == 0)
            parent_sic = line.substr(std::string("parent_sic:").size());
        else if (line.find("file_count:") == 0)
            file_count = std::stoi(line.substr(std::string("file_count:").size()));
        else if (line.find("total_size:") == 0)
            total_size = std::stoul(line.substr(std::string("total_size:").size()));
    }
    (void)token; (void)message;
    std::string current_head = names.getHead();
    if (current_head != parent_sic && !parent_sic.empty()) {
        return "HOOK_REJECTED:HEAD has moved|server_head:" + current_head +
            "|your_head:" + parent_sic;
    }
    if (names.hasName(name) && name != "HEAD") {
        return "HOOK_REJECTED:name already exists|name:" + name;
    }
    std::string nonce = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    return "HOOK_READY:nonce=" + nonce + "|file_count=" + std::to_string(file_count) +
        "|total_size=" + std::to_string(total_size);
}

std::string VaultReceiver::handleHookData(const std::string& params) {
    size_t sep = params.find("|sic:");
    if (sep == std::string::npos) {
        return "HOOK_FAILED:malformed payload";
    }
    std::string payload_str = params.substr(0, sep);
    std::vector<uint8_t> payload_data(payload_str.begin(), payload_str.end());
    auto [version, deltas] = unpack_payload(payload_data);
    std::string author_id = version.author_id;
    if (author_id.empty()) {
        auto local = AuthorRegistry::loadLocalAuthor();
        author_id = local.id;
    }
    int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    version.timestamp = timestamp;
    version.author_id = author_id;
    std::string parent_sic = names.getHead();
    if (parent_sic.empty()) parent_sic = "genesis";
    version.parent_sic = parent_sic;
    std::string tree_hash = version.tree_hash;
    if (tree_hash.empty()) {
        Tree tree;
        tree.path = "";
        for (const auto& d : deltas) {
            if (d.operation != FileDelta::Op::DELETED) {
                std::string bh = store.storeBlob(d.content);
                tree.entries.push_back({d.path, bh, 0644});
            }
        }
        tree_hash = store.storeTree(tree);
        version.tree_hash = tree_hash;
    }
    version.sic = generate_sic(version.parent_sic, version.tree_hash, version.author_id,
                                version.timestamp, version.name);
    version.short_sic = short_sic(version.sic);
    for (const auto& d : deltas) {
        version.changed_files.push_back(d.path);
        if (d.operation != FileDelta::Op::DELETED) {
            store.storeBlob(d.content);
        }
    }
    store.storeVersion(version);
    chain.setHead(version.sic);
    names.registerName(version.name, version.sic);
    names.setHead(version.sic);
    std::ostringstream resp;
    resp << "HOOK_DONE:sic=" << version.sic
         << "|short_sic=" << version.short_sic
         << "|parent_sic=" << version.parent_sic
         << "|timestamp=" << version.timestamp
         << "|name=" << version.name;
    for (const auto& cf : version.changed_files)
        resp << "|changed_file=" << cf;
    return resp.str();
}

std::string VaultReceiver::handleGetReq(const std::string& params) {
    std::string sic_or_name;
    if (params.find("sic_or_name:") == 0)
        sic_or_name = params.substr(std::string("sic_or_name:").size());
    else
        sic_or_name = params;
    std::string full_sic = names.resolve(sic_or_name);
    if (full_sic.empty() || !store.versionExists(full_sic)) {
        return "GET_NOT_FOUND:version not found|sic_or_name:" + sic_or_name;
    }
    Version v = store.loadVersion(full_sic);
    Tree tree = store.loadTree(v.tree_hash);
    std::vector<FileDelta> deltas;
    for (const auto& entry : tree.entries) {
        FileDelta d;
        d.operation = FileDelta::Op::ADDED;
        d.path = entry.name;
        d.blob_hash = entry.blob_hash;
        d.content = store.loadBlob(entry.blob_hash);
        deltas.push_back(std::move(d));
    }
    auto payload = pack_payload(deltas, v);
    std::ostringstream resp;
    resp << "GET_READY:payload_size=" << payload.size()
         << "|sic=" << v.sic
         << "|short_sic=" << v.short_sic
         << "|name=" << v.name;
    resp << "|PAYLOAD:";
    resp.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    return resp.str();
}

std::string VaultReceiver::handleGetAck(const std::string& params) {
    (void)params;
    return "GET_SUCCESS";
}

std::string VaultReceiver::handleSicQuery(const std::string& params) {
    std::string target;
    if (params.find("target:") == 0)
        target = params.substr(std::string("target:").size());
    else
        target = params;
    if (target == "--log") {
        auto versions = store.listVersions();
        std::ostringstream resp;
        resp << "SIC_RESPONSE:type=log|count=" << versions.size();
        std::string head_sic = names.getHead();
        for (const auto& v : versions) {
            resp << "|version:"
                 << "name=" << v.name
                 << ",sic=" << v.sic
                 << ",short_sic=" << v.short_sic
                 << ",timestamp=" << v.timestamp
                 << ",author_id=" << v.author_id
                 << ",parent_sic=" << v.parent_sic
                 << ",message=" << v.message;
            if (v.sic == head_sic) resp << ",HEAD";
        }
        return resp.str();
    }
    std::string full_sic = names.resolve(target);
    if (full_sic.empty() || !store.versionExists(full_sic)) {
        return "SIC_RESPONSE:not_found|target:" + target;
    }
    Version v = store.loadVersion(full_sic);
    std::ostringstream resp;
    resp << "SIC_RESPONSE:type=single"
         << "|sic=" << v.sic
         << "|short_sic=" << v.short_sic
         << "|name=" << v.name
         << "|parent_sic=" << v.parent_sic
         << "|tree_hash=" << v.tree_hash
         << "|author_id=" << v.author_id
         << "|timestamp=" << v.timestamp
         << "|message=" << v.message;
    return resp.str();
}

std::string VaultReceiver::handleSyncReq(const std::string& params) {
    std::string local_head;
    if (params.find("local_head:") == 0)
        local_head = params.substr(std::string("local_head:").size());
    else
        local_head = params;
    auto all_versions = store.listVersions();
    std::vector<Version> new_versions;
    bool found = (local_head == "genesis");
    for (const auto& v : all_versions) {
        if (found) new_versions.push_back(v);
        else if (v.sic == local_head || v.name == local_head) found = true;
    }
    std::string server_head = names.getHead();
    std::ostringstream resp;
    resp << "SYNC_RESPONSE:count=" << new_versions.size()
         << "|server_head=" << server_head;
    for (const auto& v : new_versions) {
        resp << "|version:"
             << "name=" << v.name
             << ",sic=" << v.sic
             << ",short_sic=" << v.short_sic
             << ",timestamp=" << v.timestamp
             << ",author_id=" << v.author_id
             << ",parent_sic=" << v.parent_sic
             << ",message=" << v.message;
    }
    return resp.str();
}

std::string VaultReceiver::handleVerifyReq(const std::string& params) {
    std::string until_sic;
    if (params.find("until:") == 0)
        until_sic = params.substr(std::string("until:").size());
    auto versions = store.listVersions();
    std::ostringstream resp;
    resp << "VERIFY_RESPONSE:count=" << versions.size();
    bool chain_ok = true;
    for (const auto& v : versions) {
        std::string expected = generate_sic(v.parent_sic, v.tree_hash,
                                              v.author_id, v.timestamp, v.name);
        bool ok = (expected == v.sic);
        if (!ok) chain_ok = false;
        resp << "|version_check:"
             << "name=" << v.name
             << ",sic=" << v.sic
             << ",status=" << (ok ? "OK" : "FAILED");
        if (!until_sic.empty() && v.sic == until_sic) break;
    }
    resp << "|chain_status=" << (chain_ok ? "INTACT" : "TAMPERED");
    return resp.str();
}

std::string VaultReceiver::handleRollbackReq(const std::string& params) {
    std::string target;
    if (params.find("target:") == 0)
        target = params.substr(std::string("target:").size());
    else
        target = params;
    std::string full_sic = names.resolve(target);
    if (full_sic.empty() || !store.versionExists(full_sic)) {
        return "ROLLBACK_RESPONSE:failed|reason=version not found|target:" + target;
    }
    chain.setHead(full_sic);
    names.setHead(full_sic);
    Version v = store.loadVersion(full_sic);
    std::ostringstream resp;
    resp << "ROLLBACK_RESPONSE:success"
         << "|sic=" << v.sic
         << "|short_sic=" << v.short_sic
         << "|name=" << v.name;
    return resp.str();
}
