#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class NameRegistry {
public:
    explicit NameRegistry(const std::string& storage_path);

    void registerName(const std::string& name, const std::string& full_sic);
    void setHead(const std::string& full_sic);
    std::string resolve(const std::string& name_or_sic) const;
    std::string getHead() const;
    std::string getNameForSic(const std::string& full_sic) const;
    std::vector<std::pair<std::string, std::string>> getAll() const;
    bool hasName(const std::string& name) const;
    void remove(const std::string& name);
    void save() const;
    void load();

private:
    std::string path_;
    std::unordered_map<std::string, std::string> names_;
    std::string filePath() const;
};
