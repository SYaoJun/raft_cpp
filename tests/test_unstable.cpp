#include <gtest/gtest.h>
#include <raft-kv/raft/unstable.h>

using namespace kv;

static proto::SnapshotPtr newSnapshot(uint64_t index, uint64_t term) {
  proto::SnapshotPtr s(new proto::Snapshot());
  s->metadata.index = index;
  s->metadata.term = term;
  return s;
}

static bool snapshot_cmp(const proto::Snapshot& left, const proto::Snapshot& right) {
  if (left.data != right.data) {
    return false;
  }

  if (left.metadata.index != right.metadata.index) {
    return false;
  }

  if (left.metadata.term != right.metadata.term) {
    return false;
  }

  if (left.metadata.conf_state.nodes != right.metadata.conf_state.nodes) {
    return false;
  }

  if (left.metadata.conf_state.learners != right.metadata.conf_state.learners) {
    return false;
  }

  return true;
}

static proto::EntryPtr newEntry(uint64_t index, uint64_t term) {
  proto::EntryPtr entry(new proto::Entry());
  entry->index = index;
  entry->term = term;
  return entry;
}

TEST(unstable, first_index) {
  {
    uint64_t windex = 0;

    Unstable unstable(5);
    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    uint64_t index;
    bool ok;
    unstable.maybe_first_index(index, ok);
    ASSERT_FALSE(ok);
    ASSERT_TRUE(index == windex);
  }

  {
    uint64_t windex = 0;

    Unstable unstable(5);
    unstable.snapshot_ = nullptr;
    unstable.entries_.clear();

    uint64_t index;
    bool ok;
    unstable.maybe_first_index(index, ok);
    ASSERT_FALSE(ok);
    ASSERT_TRUE(index == windex);
  }

  {
    uint64_t windex = 5;

    Unstable unstable(5);
    unstable.snapshot_ = newSnapshot(4, 1);

    unstable.entries_.push_back(newEntry(5, 1));

    uint64_t index;
    bool ok;
    unstable.maybe_first_index(index, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(index == windex);
  }

  {
    uint64_t windex = 5;

    Unstable unstable(5);
    unstable.snapshot_ = newSnapshot(4, 1);;

    unstable.entries_.clear();

    uint64_t index;
    bool ok;
    unstable.maybe_first_index(index, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(index == windex);
  }
}

TEST(unstable, last_index) {
  {
    uint64_t windex = 5;

    Unstable unstable(5);
    unstable.snapshot_ = newSnapshot(4, 1);;

    unstable.entries_.push_back(newEntry(5, 1));

    uint64_t index;
    bool ok;
    unstable.maybe_last_index(index, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(index == windex);
  }

  {
    uint64_t windex = 5;

    Unstable unstable(5);
    unstable.snapshot_ = nullptr;

    proto::EntryPtr entry(new proto::Entry());
    entry->index = 5;
    entry->term = 1;
    unstable.entries_.push_back(entry);

    uint64_t index;
    bool ok;
    unstable.maybe_last_index(index, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(index == windex);
  }

  {
    uint64_t windex = 4;

    Unstable unstable(5);
    unstable.snapshot_ = newSnapshot(4, 1);

    unstable.entries_.clear();

    uint64_t index;
    bool ok;
    unstable.maybe_last_index(index, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(index == windex);
  }

  {
    uint64_t windex = 0;

    Unstable unstable(5);

    uint64_t index;
    bool ok;
    unstable.maybe_last_index(index, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(index == windex);
  }
}

TEST(unstalbe, term) {
  {
    uint64_t wterm = 1;

    Unstable unstable(5);

    unstable.entries_.push_back(newEntry(5, 1));

    uint64_t term;
    bool ok;
    unstable.maybe_term(5, term, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(term == wterm);
  }

  {
    uint64_t wterm = 0;

    Unstable unstable(5);

    proto::EntryPtr entry(new proto::Entry());
    entry->index = 5;
    entry->term = 1;
    unstable.entries_.push_back(entry);

    uint64_t term;
    bool ok;
    unstable.maybe_term(6, term, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(term == wterm);
  }

  {
    uint64_t wterm = 0;

    Unstable unstable(5);

    unstable.entries_.push_back(newEntry(5, 1));

    uint64_t term;
    bool ok;
    unstable.maybe_term(4, term, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(term == wterm);
  }

  {
    uint64_t wterm = 1;

    Unstable unstable(5);

    proto::EntryPtr entry(new proto::Entry());
    entry->index = 5;
    entry->term = 1;
    unstable.entries_.push_back(entry);

    unstable.snapshot_ = newSnapshot(4, 1);

    uint64_t term;
    bool ok;
    unstable.maybe_term(5, term, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(term == wterm);
  }

  {
    Unstable unstable(5);

    unstable.entries_.push_back(newEntry(5, 1));

    unstable.snapshot_ = newSnapshot(4, 1);

    uint64_t term;
    bool ok;
    unstable.maybe_term(6, term, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(term == 0);
  }

  {
    Unstable unstable(5);

    unstable.entries_.push_back(newEntry(5, 1));

    unstable.snapshot_ = newSnapshot(4, 1);

    uint64_t term;
    bool ok;
    unstable.maybe_term(4, term, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(term == 1);
  }

  {
    Unstable unstable(5);

    unstable.entries_.push_back(newEntry(5, 1));

    unstable.snapshot_ = newSnapshot(4, 1);

    uint64_t term;
    bool ok;
    unstable.maybe_term(3, term, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(term == 0);
  }

  {
    Unstable unstable(5);

    unstable.entries_.clear();

    unstable.snapshot_ = newSnapshot(4, 1);

    uint64_t term;
    bool ok;
    unstable.maybe_term(5, term, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(term == 0);
  }

  {
    Unstable unstable(5);

    unstable.entries_.clear();

    unstable.snapshot_ = newSnapshot(4, 1);

    uint64_t term;
    bool ok;
    unstable.maybe_term(4, term, ok);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(term == 1);
  }

  {
    Unstable unstable(5);

    unstable.entries_.clear();

    uint64_t term;
    bool ok;
    unstable.maybe_term(5, term, ok);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(term == 0);
  }
}

TEST(unstable, restore) {
  Unstable unstable(5);

  unstable.entries_.clear();

  unstable.snapshot_ = newSnapshot(4, 1);

  auto s = newSnapshot(6, 2);

  unstable.restore(s);

  ASSERT_TRUE(unstable.offset_ == s->metadata.index + 1);
  ASSERT_TRUE(unstable.entries_.empty());
  ASSERT_TRUE(snapshot_cmp(*unstable.snapshot_, *s));
}

TEST(unstable, stable) {
  {
    Unstable unstable(0);

    unstable.stable_to(5, 1);
    ASSERT_TRUE(unstable.offset_ == 0);
    ASSERT_TRUE(unstable.entries_.size() == 0);
  }

  {
    // stable to the first entry
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    unstable.stable_to(5, 1);
    ASSERT_TRUE(unstable.offset_ == 6);
    ASSERT_TRUE(unstable.entries_.size() == 0);
  }

  {
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));
    unstable.entries_.push_back(newEntry(6, 1));

    unstable.stable_to(5, 1);
    ASSERT_TRUE(unstable.offset_ == 6);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }

  {
    // stable to the first entry and term mismatch
    Unstable unstable(6);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(6, 2));

    unstable.stable_to(6, 1);
    ASSERT_TRUE(unstable.offset_ == 6);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }

  {
    // stable to old entry
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    unstable.stable_to(4, 1);
    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }

  {
    // stable to old entry
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    unstable.stable_to(4, 2);
    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }

  {
    // stable to old entry
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    unstable.snapshot_ = newSnapshot(4, 1);

    unstable.stable_to(5, 1);
    ASSERT_TRUE(unstable.offset_ == 6);
    ASSERT_TRUE(unstable.entries_.size() == 0);
  }

  {
    // stable to old entry
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));
    unstable.entries_.push_back(newEntry(6, 1));

    unstable.snapshot_ = newSnapshot(4, 1);

    unstable.stable_to(5, 1);
    ASSERT_TRUE(unstable.offset_ == 6);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }

  {
    // stable to snapshot
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    unstable.snapshot_ = newSnapshot(4, 1);

    unstable.stable_to(4, 1);
    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }

  {
    // stable to old entry
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 2));

    unstable.snapshot_ = newSnapshot(4, 2);

    unstable.stable_to(4, 1);
    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(unstable.entries_.size() == 1);
  }
}

bool entry_cmp(const std::vector<proto::EntryPtr>& left, const std::vector<proto::EntryPtr>& right) {
  if (left.size() != right.size()) {
    return false;
  }

  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i]->index != right[i]->index) {
      return false;
    }

    if (left[i]->term != right[i]->term) {
      return false;
    }
  }
  return true;
}

TEST(unstable, append) {
  {
    // append to the end
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    std::vector<proto::EntryPtr> to_append;
    to_append.push_back(newEntry(6, 1));
    to_append.push_back(newEntry(7, 1));

    std::vector<proto::EntryPtr> wentries;
    wentries.push_back(newEntry(5, 1));
    wentries.push_back(newEntry(6, 1));
    wentries.push_back(newEntry(7, 1));

    unstable.truncate_and_append(std::move(to_append));

    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(entry_cmp(unstable.entries_, wentries));
  }

  {
    // replace the unstable entries
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    std::vector<proto::EntryPtr> to_append;
    to_append.push_back(newEntry(5, 2));
    to_append.push_back(newEntry(6, 2));

    std::vector<proto::EntryPtr> wentries;
    wentries.push_back(newEntry(5, 2));
    wentries.push_back(newEntry(6, 2));

    unstable.truncate_and_append(std::move(to_append));

    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(entry_cmp(unstable.entries_, wentries));
  }

  {
    // replace the unstable entries
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));

    std::vector<proto::EntryPtr> to_append;
    to_append.push_back(newEntry(4, 2));
    to_append.push_back(newEntry(5, 2));
    to_append.push_back(newEntry(6, 2));

    std::vector<proto::EntryPtr> wentries;
    wentries.push_back(newEntry(4, 2));
    wentries.push_back(newEntry(5, 2));
    wentries.push_back(newEntry(6, 2));

    unstable.truncate_and_append(std::move(to_append));

    ASSERT_TRUE(unstable.offset_ == 4);
    ASSERT_TRUE(entry_cmp(unstable.entries_, wentries));
  }

  {
    // truncate the existing entries and append
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));
    unstable.entries_.push_back(newEntry(6, 1));
    unstable.entries_.push_back(newEntry(7, 1));

    std::vector<proto::EntryPtr> to_append;
    to_append.push_back(newEntry(6, 2));

    std::vector<proto::EntryPtr> wentries;
    wentries.push_back(newEntry(5, 1));
    wentries.push_back(newEntry(6, 2));

    unstable.truncate_and_append(std::move(to_append));

    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(entry_cmp(unstable.entries_, wentries));
  }

  {
    // truncate the existing entries and append
    Unstable unstable(5);

    unstable.snapshot_ = nullptr;
    unstable.entries_.push_back(newEntry(5, 1));
    unstable.entries_.push_back(newEntry(6, 1));
    unstable.entries_.push_back(newEntry(7, 1));

    std::vector<proto::EntryPtr> to_append;
    to_append.push_back(newEntry(7, 2));
    to_append.push_back(newEntry(8, 2));

    std::vector<proto::EntryPtr> wentries;
    wentries.push_back(newEntry(5, 1));
    wentries.push_back(newEntry(6, 1));
    wentries.push_back(newEntry(7, 2));
    wentries.push_back(newEntry(8, 2));

    unstable.truncate_and_append(std::move(to_append));

    ASSERT_TRUE(unstable.offset_ == 5);
    ASSERT_TRUE(entry_cmp(unstable.entries_, wentries));
  }
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

