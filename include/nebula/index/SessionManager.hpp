#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nebula {
namespace index {

struct Session {
    uint64_t id;
    bool active;
};

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager();

    void addSession(uint64_t id);
    void closeSession(uint64_t id);
    Session* getSession(uint64_t id);

private:
    std::unordered_map<uint64_t, Session*> sessions_;
    std::vector<Session*> activeSessions_;
};

} // namespace index
} // namespace nebula
