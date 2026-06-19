#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct Author {
    std::string id;
    std::string display_name;
    std::string public_key;
};

class AuthorRegistry {
public:
    AuthorRegistry();
    explicit AuthorRegistry(const std::string& config_path);

    void registerAuthor(const Author& author);
    Author getAuthor(const std::string& id) const;
    bool hasAuthor(const std::string& id) const;
    std::vector<Author> getAllAuthors() const;
    void remove(const std::string& id);
    void save() const;
    void load();

    static Author loadLocalAuthor();
    static std::string localAuthorPath();

private:
    std::string path_;
    std::unordered_map<std::string, Author> authors_;
};
