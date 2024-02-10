#include <mutex>
#include <map>
#include <string>

namespace poorjit {
    std::mutex& jitmtx() {
        static std::mutex mtx;
        return mtx;
    }
    std::map<std::string, std::mutex>& file_mtx() {
        static std::map<std::string, std::mutex> mtx;
        return mtx;
    };
}
