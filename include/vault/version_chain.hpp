#pragma once
#include "object_store.hpp"
#include <string>
#include <vector>

class VersionChain {
public:
    explicit VersionChain(ObjectStore& store);

    Version append(const Version& version);
    Version getHead() const;
    Version getBySic(const std::string& sic) const;
    Version getByName(const std::string& name) const;
    std::vector<Version> getHistory() const;
    std::vector<Version> getSince(const std::string& sic) const;
    size_t getLength() const;
    bool rollback(const std::string& sic);
    bool verifyIntegrity(const std::string& until_sic = "") const;

    void setHead(const std::string& sic);
    std::string getHeadSic() const;

private:
    ObjectStore& store_;
    std::string storage_path_;
    std::string headPath() const;
};
