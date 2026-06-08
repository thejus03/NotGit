#include "ObjectStore.hpp"
#include "Objects/BaseObject.hpp"
#include "Objects/BlobObject.hpp"
#include "Objects/TreeObject.hpp"
#include "Objects/CommitObject.hpp"
#include <filesystem>
#include <memory>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <zlib.h>

static std::vector<TreeEntry> parseTreeContent(const std::string& content);
static Commit parseCommitContent(const std::string& content);

ObjectStore::ObjectStore(const std::filesystem::path& objectDirPath) : objectDirPath_(objectDirPath) {}

std::unique_ptr<BaseObject> ObjectStore::read(const std::string& sha1) const {
    if (!exists(sha1))
        return nullptr;
    std::filesystem::path objectFilePath = objectFilePathFromSHA_(sha1);
    std::ifstream file (objectFilePath, std::ios::binary);
    std::string compressedData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::string decompressedData = decompress_(compressedData);
    size_t spacePos = decompressedData.find(' ');
    size_t nullPos = decompressedData.find('\0');

    std::string type = decompressedData.substr(0, spacePos);
    size_t dataLen = std::stoul(decompressedData.substr(spacePos + 1, nullPos - spacePos - 1));
    std::string data = decompressedData.substr(nullPos + 1);
    if (dataLen != data.size())
        throw std::runtime_error("object size mismatch");

    if (type == "blob") {
        return std::make_unique<BlobObject>(data);
    } else if (type == "tree") {
        return std::make_unique<TreeObject>(parseTreeContent(data));
    } else if (type == "commit") {
        return std::make_unique<CommitObject>(parseCommitContent(data));
    } else {
        throw std::runtime_error("unknown object type: " + type);
    }
}

std::string ObjectStore::write(const BaseObject& obj) const {
    const std::string sha1 = obj.sha1();
    std::filesystem::path objectFilePath = objectFilePathFromSHA_(sha1);
    if (!std::filesystem::exists(objectFilePath)) {
        auto parentDir = objectFilePath.parent_path();
        std::filesystem::create_directories(parentDir);

        std::string data = obj.header() + obj.rawData();
        std::string compressedData = compress_(data);

        std::ofstream file(objectFilePath, std::ios::binary);
        if (!file)
            throw std::runtime_error("failed to open object file for writing: " + objectFilePath.string());
        file.write(compressedData.data(), compressedData.size());
    }

    return sha1;
}

bool ObjectStore::exists(const std::string& sha1) const {
    std::filesystem::path objectFilePath = objectFilePathFromSHA_(sha1);
    return std::filesystem::exists(objectFilePath);
}

std::filesystem::path ObjectStore::objectFilePathFromSHA_(const std::string& sha1) const {
    return objectDirPath_ / sha1.substr(0, 2) / sha1.substr(2);
}

std::string ObjectStore::compress_(const std::string& data) const {
    z_stream zs{};
    deflateInit(&zs, Z_BEST_COMPRESSION);

    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();

    std::string result;
    char buffer[4096];
    int ret;
    do {
        zs.next_out = (Bytef*)buffer;
        zs.avail_out = sizeof(buffer);
        ret = deflate(&zs, Z_FINISH);
        result.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END)
        throw std::runtime_error("zlib compression failed");

    return result;
}

std::string ObjectStore::decompress_(const std::string& compressed) const {
    z_stream zs{};
    inflateInit(&zs);

    zs.next_in = (Bytef*)compressed.data();
    zs.avail_in = compressed.size();

    std::string result;
    char buffer[4096];
    int ret;
    do {
        zs.next_out = (Bytef*)buffer;
        zs.avail_out = sizeof(buffer);
        ret = inflate(&zs, Z_NO_FLUSH);
        result.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END)
        throw std::runtime_error("zlib decompression failed");

    return result;
}

static std::vector<TreeEntry> parseTreeContent(const std::string& content) {
    std::vector<TreeEntry> entries;
    size_t i = 0;
    while (i < content.size()) {
        size_t spacePos = content.find(' ', i);
        size_t nullPos = content.find('\0', spacePos);

        std::string mode = content.substr(i, spacePos - i);
        std::string name = content.substr(spacePos + 1, nullPos - spacePos - 1);

        // 20 raw binary bytes after the null byte
        std::string binarySHA = content.substr(nullPos + 1, 20);

        // convert binary SHA to hex
        std::ostringstream ss;
        for (unsigned char byte : binarySHA)
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;

        entries.push_back({name, mode, ss.str()});
        i = nullPos + 1 + 20; // move to next entry
    }
    return entries;
}

static Commit parseCommitContent(const std::string& content) {
    Commit commit;
    std::istringstream ss(content);
    std::string line;

    while (std::getline(ss, line)) {
        size_t spacePos = line.find(' ');
        std::string fieldType = line.substr(0, spacePos);
        std::string info = line.substr(spacePos + 1);
        if (fieldType == "tree") {
            commit.treeSHA = std::move(info);
        } else if (fieldType == "parent") {
            commit.parentCommitSHA = std::move(info); 
        } else if (fieldType == "author") {
            commit.author = std::move(info);
        } else if (fieldType == "committer") {
            commit.committer = std::move(info);
        } else if (fieldType.empty()) {
            // if empty line means that next line onward is commit message
            // we will parse commit message seperately
            break;
        } else {
            throw std::runtime_error("Weird order for commit content");
        }
    }

    // Parse commit message
    std::string commitMsg;
    while (std::getline(ss, line))
        commitMsg += line + '\n';
    if (!commitMsg.empty() && commitMsg.back() == '\n')
        commitMsg.pop_back();
    commit.commitMsg = std::move(commitMsg);

    return commit;
}

