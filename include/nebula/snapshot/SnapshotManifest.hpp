#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <span>

namespace nebula {
namespace snapshot {

using SnapshotID = uint64_t;

struct SnapshotNode {
    uint64_t entryId{0};
    std::string path;
    uint64_t size{0};
    uint32_t checksum{0};
    uint64_t modifiedTime{0};
    std::vector<uint64_t> blockIds;

    bool operator==(const SnapshotNode& other) const {
        return entryId == other.entryId && path == other.path &&
               size == other.size && checksum == other.checksum &&
               modifiedTime == other.modifiedTime && blockIds == other.blockIds;
    }
};

class SnapshotManifest {
public:
    SnapshotManifest() = default;
    SnapshotManifest(SnapshotID id, SnapshotID parentId, std::string label, uint64_t timestamp = 0);

    void addNode(SnapshotNode node);
    bool removeNode(const std::string& path);
    [[nodiscard]] std::optional<SnapshotNode> findNode(const std::string& path) const;
    [[nodiscard]] bool hasNode(const std::string& path) const;

    [[nodiscard]] SnapshotID id() const noexcept { return id_; }
    [[nodiscard]] SnapshotID parentId() const noexcept { return parentId_; }
    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    [[nodiscard]] uint64_t timestamp() const noexcept { return timestamp_; }

    [[nodiscard]] size_t nodeCount() const noexcept { return nodes_.size(); }
    [[nodiscard]] uint64_t totalSize() const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, SnapshotNode>& nodes() const noexcept { return nodes_; }

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static std::optional<SnapshotManifest> deserialize(std::span<const uint8_t> bytes);

    void clear() noexcept;

private:
    SnapshotID id_{0};
    SnapshotID parentId_{0};
    std::string label_;
    uint64_t timestamp_{0};
    std::unordered_map<std::string, SnapshotNode> nodes_;
};

} // namespace snapshot
} // namespace nebula
