#include <gtest/gtest.h>
#include "nebula/snapshot/SnapshotManifest.hpp"
#include "nebula/snapshot/DeltaEngine.hpp"
#include "nebula/snapshot/PatchApplier.hpp"
#include "nebula/snapshot/SnapshotManager.hpp"

using namespace nebula::snapshot;

// --- SnapshotManifest Tests ---

TEST(SnapshotManifestTest, AddFindAndRemoveNodes) {
    SnapshotManifest manifest(1, 0, "Initial Snapshot", 1000);

    SnapshotNode node1{10, "/docs/readme.txt", 100, 0x1234, 1000, {1, 2}};
    SnapshotNode node2{11, "/images/logo.png", 500, 0x5678, 1000, {3, 4, 5}};

    manifest.addNode(node1);
    manifest.addNode(node2);

    EXPECT_EQ(manifest.nodeCount(), 2);
    EXPECT_EQ(manifest.totalSize(), 600);
    EXPECT_TRUE(manifest.hasNode("/docs/readme.txt"));

    auto found = manifest.findNode("/docs/readme.txt");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, node1);

    EXPECT_TRUE(manifest.removeNode("/images/logo.png"));
    EXPECT_EQ(manifest.nodeCount(), 1);
    EXPECT_FALSE(manifest.hasNode("/images/logo.png"));
}

TEST(SnapshotManifestTest, SerializationRoundTrip) {
    SnapshotManifest original(42, 10, "Release v2.0", 1700000000);

    SnapshotNode n1{1, "/bin/exec", 4096, 0xAAAA, 1700000000, {100, 101, 102}};
    SnapshotNode n2{2, "/etc/config.json", 256, 0xBBBB, 1700000000, {200}};

    original.addNode(n1);
    original.addNode(n2);

    auto bytes = original.serialize();
    EXPECT_FALSE(bytes.empty());

    auto restored = SnapshotManifest::deserialize(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->id(), 42);
    EXPECT_EQ(restored->parentId(), 10);
    EXPECT_EQ(restored->label(), "Release v2.0");
    EXPECT_EQ(restored->nodeCount(), 2);
    EXPECT_EQ(restored->totalSize(), 4096 + 256);
    EXPECT_EQ(*restored->findNode("/bin/exec"), n1);
}

// --- DeltaEngine Tests ---

TEST(DeltaEngineTest, ComputeAddModifyDeleteDelta) {
    SnapshotManifest v1(1, 0, "v1");
    v1.addNode(SnapshotNode{1, "/file_unchanged.txt", 100, 0x111, 100, {1}});
    v1.addNode(SnapshotNode{2, "/file_to_modify.txt", 200, 0x222, 100, {2, 3}});
    v1.addNode(SnapshotNode{3, "/file_to_delete.txt", 300, 0x333, 100, {4}});

    SnapshotManifest v2(2, 1, "v2");
    v2.addNode(SnapshotNode{1, "/file_unchanged.txt", 100, 0x111, 100, {1}}); // same
    v2.addNode(SnapshotNode{2, "/file_to_modify.txt", 250, 0x999, 200, {2, 5}}); // modified
    v2.addNode(SnapshotNode{4, "/file_new.txt", 400, 0x444, 200, {6, 7}}); // added

    auto delta = DeltaEngine::computeDelta(v1, v2);
    EXPECT_EQ(delta.baseSnapshotId, 1);
    EXPECT_EQ(delta.targetSnapshotId, 2);
    EXPECT_EQ(delta.addedCount(), 1);
    EXPECT_EQ(delta.modifiedCount(), 1);
    EXPECT_EQ(delta.deletedCount(), 1);

    auto reused = DeltaEngine::computeReusedBlocks(delta);
    ASSERT_EQ(reused.size(), 1);
    EXPECT_EQ(reused[0], 2); // Block 2 was kept in file_to_modify

    // Delta serialization
    auto deltaBytes = delta.serialize();
    auto restoredDelta = SnapshotDelta::deserialize(deltaBytes);
    ASSERT_TRUE(restoredDelta.has_value());
    EXPECT_EQ(restoredDelta->baseSnapshotId, 1);
    EXPECT_EQ(restoredDelta->targetSnapshotId, 2);
    EXPECT_EQ(restoredDelta->entries.size(), 3);
}

// --- PatchApplier Tests ---

TEST(PatchApplierTest, ForwardRollReconstructsTarget) {
    SnapshotManifest base(1, 0, "Base");
    base.addNode(SnapshotNode{1, "/keep.txt", 50, 0x1, 100, {1}});
    base.addNode(SnapshotNode{2, "/update.txt", 50, 0x2, 100, {2}});
    base.addNode(SnapshotNode{3, "/del.txt", 50, 0x3, 100, {3}});

    SnapshotManifest target(2, 1, "Target");
    target.addNode(SnapshotNode{1, "/keep.txt", 50, 0x1, 100, {1}});
    target.addNode(SnapshotNode{2, "/update.txt", 80, 0x9, 200, {4}});
    target.addNode(SnapshotNode{4, "/created.txt", 120, 0x5, 200, {5}});

    auto delta = DeltaEngine::computeDelta(base, target);
    auto reconstructed = PatchApplier::applyPatch(base, delta, "Reconstructed");

    ASSERT_TRUE(reconstructed.has_value());
    EXPECT_EQ(reconstructed->id(), 2);
    EXPECT_EQ(reconstructed->nodeCount(), 3);
    EXPECT_TRUE(reconstructed->hasNode("/keep.txt"));
    EXPECT_TRUE(reconstructed->hasNode("/update.txt"));
    EXPECT_TRUE(reconstructed->hasNode("/created.txt"));
    EXPECT_FALSE(reconstructed->hasNode("/del.txt"));
    EXPECT_EQ(reconstructed->findNode("/update.txt")->size, 80);
}

// --- SnapshotManager Tests ---

TEST(SnapshotManagerTest, LifecycleAndLineage) {
    SnapshotManager sm;

    std::vector<SnapshotNode> v1Nodes = {
        SnapshotNode{1, "/a.txt", 10, 0x1, 1, {1}}
    };
    SnapshotID s1 = sm.createSnapshot("v1", v1Nodes);
    EXPECT_EQ(s1, 1);
    EXPECT_EQ(sm.currentSnapshotId(), 1);

    std::vector<SnapshotNode> v2Nodes = {
        SnapshotNode{1, "/a.txt", 10, 0x1, 1, {1}},
        SnapshotNode{2, "/b.txt", 20, 0x2, 2, {2}}
    };
    SnapshotID s2 = sm.createSnapshot("v2", v2Nodes);
    EXPECT_EQ(s2, 2);
    EXPECT_EQ(sm.currentSnapshotId(), 2);

    auto snap2 = sm.getSnapshot(s2);
    ASSERT_TRUE(snap2.has_value());
    EXPECT_EQ(snap2->parentId(), s1);

    // Delta query
    auto deltaOpt = sm.createDelta(s1, s2);
    ASSERT_TRUE(deltaOpt.has_value());
    EXPECT_EQ(deltaOpt->addedCount(), 1);

    // Rollback to s1
    EXPECT_TRUE(sm.checkoutSnapshot(s1));
    EXPECT_EQ(sm.currentSnapshotId(), s1);

    auto list = sm.listSnapshots();
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].id(), s1);
    EXPECT_EQ(list[1].id(), s2);
}
