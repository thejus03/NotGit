#pragma once

#include <optional>

#include "BaseObject.hpp"

struct Commit {
    std::string treeSHA;
    std::optional<std::string> parentCommitSHA;
    std::string author;
    std::string committer;
    std::string commitMsg;
};

class CommitObject : public BaseObject {
  public:
    CommitObject(const Commit& commit);

    std::string header() const override;
    std::string rawData() const override;

    std::optional<std::string> parentCommitSHA() const;
    std::string author() const;
    std::string commitMsg() const;
    std::string treeSHA() const;

  private:
    std::string treeSHA_;
    std::optional<std::string> parentCommitSHA_;
    std::string author_;
    std::string committer_;
    std::string commitMsg_;
    std::string data_;

    std::string buildRawData() const;
};
