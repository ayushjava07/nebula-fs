#include "nebula/index/SessionManager.hpp"
#include <algorithm>

namespace nebula {
namespace index {

SessionManager::~SessionManager() {
    for (auto& [id, session] : sessions_) {
        delete session;
    }
    sessions_.clear();
    activeSessions_.clear();
}

void SessionManager::addSession(uint64_t id) {
    // Bug #2: TOCTOU race - two threads can both pass this check
    if (sessions_.find(id) != sessions_.end()) {
        return;
    }
    auto* session = new Session{id, true};
    sessions_[id] = session;
    activeSessions_.push_back(session);
}

void SessionManager::closeSession(uint64_t id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) return;

    // Bug #15: deletes the session and erases from sessions_,
    // but activeSessions_ still holds a dangling pointer
    delete it->second;
    sessions_.erase(it);
    // activeSessions_ NOT cleaned up -> use-after-free / double-free
}

Session* SessionManager::getSession(uint64_t id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) return nullptr;
    return it->second;
}

} // namespace index
} // namespace nebula
