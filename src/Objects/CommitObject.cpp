
#include "CommitObject.hpp"
#include <format>

CommitObject::CommitObject(const std::string& treeSHA, std::optional<std::string> parentCommitSHA, 
    const std::string& author, const std::string& committer, const std::string& commitMsg) 
    :
    treeSHA_(treeSHA),
    parentCommitSHA_(parentCommitSHA),
    author_(author),
    committer_(committer),
    commitMsg_(commitMsg),
    data_(buildRawData()) {}

std::string CommitObject::header() const {
    return std::format("commit {}", data_.size()) + '\0';
}

std::string CommitObject::rawData() const { return data_; }


std::string CommitObject::buildRawData() const {
    std::string res;
    res += std::format("tree {}\n", treeSHA_);
    if (parentCommitSHA_.has_value())
        res += std::format("parent {}\n", parentCommitSHA_.value());
    res += std::format("author {}\n", author_);
    res += std::format("committer {}\n", committer_);
    res += '\n';
    res += std::format("{}\n", commitMsg_);

    return res;
}