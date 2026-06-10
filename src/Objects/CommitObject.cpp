
#include "CommitObject.hpp"

#include <format>

CommitObject::CommitObject(const Commit& commit)
    : treeSHA_(commit.treeSHA),
      parentCommitSHA_(commit.parentCommitSHA),
      author_(commit.author),
      committer_(commit.committer),
      commitMsg_(commit.commitMsg),
      data_(buildRawData()) {}

std::string CommitObject::header() const {
    return std::format("commit {}", data_.size()) + '\0';
}

std::string CommitObject::rawData() const { return data_; }

std::optional<std::string> CommitObject::parentCommitSHA() const {
    return parentCommitSHA_;
}
std::string CommitObject::author() const { return author_; }
std::string CommitObject::commitMsg() const { return commitMsg_; }
std::string CommitObject::treeSHA() const { return treeSHA_; }

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