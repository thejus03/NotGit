
#include <string>

class Object {
    virtual std::string header() const = 0;
    virtual std::string rawData() const = 0;
    std::string sha1() const;
};