
#include <string>

class BaseObject {
    virtual std::string header() const = 0;
    virtual std::string rawData() const = 0;
    std::string sha1() const;
    virtual ~BaseObject() = default;
};