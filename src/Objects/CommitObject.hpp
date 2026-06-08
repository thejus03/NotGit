#pragma once

#include "BaseObject.hpp"
#include <optional>

class CommitObject : public BaseObject {
public:
    CommitObject(const std::string& treeSHA, std::optional<std::string> parentCommitSHA, 
        const std::string& author, const std::string& committer, const std::string& commitMsg);

    std::string header() const override;
    std::string rawData() const override;
private:
    std::string treeSHA_;
    std::optional<std::string> parentCommitSHA_;
    std::string author_;
    std::string committer_;
    std::string commitMsg_;
    std::string data_;

    std::string buildRawData() const;
};
