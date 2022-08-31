#include <gtest/gtest.h>
#include <raft-kv/raft/raft_log.h>
#include <raft-kv/common/log.h>

using namespace kv;

static proto::EntryPtr newEntry(uint64_t index, uint64_t term) {
  proto::EntryPtr e(new proto::Entry());
  e->index = index;
  e->term = term;
  return e;
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

TEST(raftlog, conflict) {
  std::vector<proto::EntryPtr> previousEnts;
  previousEnts.push_back(newEntry(1, 1));
  previousEnts.push_back(newEntry(2, 2));
  previousEnts.push_back(newEntry(3, 3));

  struct Test {
    std::vector<proto::EntryPtr> ents;
    uint64_t wconflict;
  };

  std::vector<Test> tests;

  // no conflict, empty ent
  {

    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(1, 1));
    ents.push_back(newEntry(2, 2));
    tests.push_back(Test{.ents = ents, .wconflict = 0});
  }


  // no conflict
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(1, 1));
    ents.push_back(newEntry(2, 2));
    ents.push_back(newEntry(3, 3));
    tests.push_back(Test{.ents = ents, .wconflict = 0});
  }

  // no conflict, but has new entries
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(1, 1));
    ents.push_back(newEntry(2, 2));
    ents.push_back(newEntry(3, 3));
    ents.push_back(newEntry(4, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 4});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(2, 2));
    ents.push_back(newEntry(3, 3));
    ents.push_back(newEntry(4, 4));
    ents.push_back(newEntry(5, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 4});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(3, 3));
    ents.push_back(newEntry(4, 4));
    ents.push_back(newEntry(5, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 4});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(4, 4));
    ents.push_back(newEntry(5, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 4});
  }


  // conflicts with existing entries
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(1, 4));
    ents.push_back(newEntry(2, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 1});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(1, 4));
    ents.push_back(newEntry(2, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 1});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(2, 1));
    ents.push_back(newEntry(3, 4));
    ents.push_back(newEntry(4, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 2});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(3, 1));
    ents.push_back(newEntry(4, 2));
    ents.push_back(newEntry(5, 4));
    ents.push_back(newEntry(6, 4));
    tests.push_back(Test{.ents = ents, .wconflict = 3});
  }

  for (size_t i = 0; i < tests.size(); ++i) {
    auto& test = tests[i];
    LOG_INFO("testing conflict %lu", i);

    MemoryStoragePtr storage(new MemoryStorage());
    RaftLog l(storage, std::numeric_limits<uint64_t>::max());

    l.append(previousEnts);

    uint64_t conflict = l.find_conflict(test.ents);

    ASSERT_TRUE(conflict == test.wconflict);
  }
}

TEST(raftlog, isuptodate) {
  std::vector<proto::EntryPtr> previousEnts;
  previousEnts.push_back(newEntry(1, 1));
  previousEnts.push_back(newEntry(2, 2));
  previousEnts.push_back(newEntry(3, 3));

  MemoryStoragePtr storage(new MemoryStorage());

  RaftLog l(storage, std::numeric_limits<uint64_t>::max());
  l.append(previousEnts);

  struct Test {
    uint64_t lastIndex;
    uint64_t term;
    bool wUpToDate;
  };

  std::vector<Test> tests;

  // greater term, ignore lastIndex
  tests.push_back(Test{.lastIndex = l.last_index() - 1, .term = 4, .wUpToDate = true});
  tests.push_back(Test{.lastIndex = l.last_index(), .term = 4, .wUpToDate = true});
  tests.push_back(Test{.lastIndex = l.last_index() + 1, .term = 4, .wUpToDate = true});

  // smaller term, ignore lastIndex
  tests.push_back(Test{.lastIndex = l.last_index() - 1, .term = 2, .wUpToDate = false});
  tests.push_back(Test{.lastIndex = l.last_index(), .term = 2, .wUpToDate = false});
  tests.push_back(Test{.lastIndex = l.last_index() + 1, .term = 2, .wUpToDate = false});

  // equal term, equal or lager lastIndex wins
  tests.push_back(Test{.lastIndex = l.last_index() - 1, .term = 3, .wUpToDate = false});
  tests.push_back(Test{.lastIndex = l.last_index(), .term = 3, .wUpToDate = true});
  tests.push_back(Test{.lastIndex = l.last_index() + 1, .term = 3, .wUpToDate = true});

  for (size_t i = 0; i < tests.size(); ++i) {
    LOG_INFO("testing isuptodate %lu", i);

    Test& test = tests[i];
    bool isUpToData = l.is_up_to_date(test.lastIndex, test.term);

    ASSERT_TRUE(isUpToData == test.wUpToDate);
  }

}

TEST(raftlog, term) {
  uint64_t offset = 100;
  uint64_t num = 100;
  MemoryStoragePtr storage(new MemoryStorage());

  proto::SnapshotPtr snapshot(new proto::Snapshot());
  snapshot->metadata.index = offset;
  snapshot->metadata.term = 1;
  storage->apply_snapshot(*snapshot);

  RaftLog log(storage, std::numeric_limits<uint64_t>::max());

  for (uint64_t i = 1; i < num; i++) {
    std::vector<proto::EntryPtr> entries;
    entries.push_back(newEntry(offset + i, i));
    log.append(std::move(entries));
  }

  struct Test {
    uint64_t Index;
    uint64_t w;
  };

  std::vector<Test> tests;

  tests.push_back(Test{.Index = offset - 1, .w =0});
  tests.push_back(Test{.Index = offset, .w =1});
  tests.push_back(Test{.Index = offset + num / 2, .w = (num / 2)});
  tests.push_back(Test{.Index = offset + num - 1, .w = (num - 1)});
  tests.push_back(Test{.Index = offset + num, .w =0});

  for (size_t i = 0; i < tests.size(); ++i) {
    LOG_INFO("testing term %lu", i);
    uint64_t term;
    log.term(tests[i].Index, term);

    ASSERT_TRUE(term == tests[i].w);
  }
}

TEST(raftlog, append) {
  std::vector<proto::EntryPtr> previousEnts;
  previousEnts.push_back(newEntry(1, 1));
  previousEnts.push_back(newEntry(2, 2));

  struct Test {
    std::vector<proto::EntryPtr> ents;
    uint64_t windex;
    std::vector<proto::EntryPtr> wents;
    uint64_t wunstable;
  };

  std::vector<Test> tests;
  {

    std::vector<proto::EntryPtr> ents;

    std::vector<proto::EntryPtr> wents;
    wents.push_back(newEntry(1, 1));
    wents.push_back(newEntry(2, 2));

    tests.push_back(Test{.ents= ents, .windex = 2, .wents = wents, .wunstable = 3});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(3, 2));

    std::vector<proto::EntryPtr> wents;
    wents.push_back(newEntry(1, 1));
    wents.push_back(newEntry(2, 2));
    wents.push_back(newEntry(3, 2));

    tests.push_back(Test{.ents= ents, .windex = 3, .wents = wents, .wunstable = 3});
  }

  {
    // conflicts with index 1
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(1, 2));

    //replace
    std::vector<proto::EntryPtr> wents;
    wents.push_back(newEntry(1, 2));

    tests.push_back(Test{.ents= ents, .windex = 1, .wents = wents, .wunstable = 1});
  }

  {
    // conflicts with index 2
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(2, 3));
    ents.push_back(newEntry(3, 3));

    std::vector<proto::EntryPtr> wents;
    wents.push_back(newEntry(1, 1));
    wents.push_back(newEntry(2, 3));
    wents.push_back(newEntry(3, 3));

    tests.push_back(Test{.ents= ents, .windex = 3, .wents = wents, .wunstable = 2});
  }

  for (size_t i = 0; i < tests.size(); ++i) {
    MemoryStoragePtr storage(new MemoryStorage());
    storage->append(previousEnts);

    RaftLog l(storage, std::numeric_limits<uint64_t>::max());
    LOG_INFO("testing append %lu", i);

    auto& t = tests[i];

    uint64_t last_index = l.append(t.ents);
    ASSERT_TRUE(last_index == t.windex);

    std::vector<proto::EntryPtr> ents;
    Status status = l.entries(1, std::numeric_limits<uint64_t>::max(), ents);
    ASSERT_TRUE(status.is_ok());

    ASSERT_TRUE(entry_cmp(ents, t.wents));

    ASSERT_TRUE(l.unstable_->offset_ == t.wunstable);

  }
}

TEST(raftlog, maybeAppend) {
  std::vector<proto::EntryPtr> previousEnts;
  previousEnts.push_back(newEntry(1, 1));
  previousEnts.push_back(newEntry(2, 2));
  previousEnts.push_back(newEntry(3, 3));

  uint64_t lastindex = 3;
  uint64_t lastterm = 3;
  uint64_t commit = 1;

  struct Test {
    uint64_t logTerm;
    uint64_t index;
    uint64_t committed;
    std::vector<proto::EntryPtr> ents;

    uint64_t wlasti;
    bool wappend;
    uint64_t wcommit;
    bool wpanic;
  };

  std::vector<Test> tests;

  {
    // not match: term is different
    Test t;
    t.logTerm = lastterm - 1;
    t.index = lastindex;
    t.committed = lastindex;
    t.ents.push_back(newEntry(lastindex + 1, 4));
    t.wlasti = 0;
    t.wappend = false;
    t.wcommit = commit;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // not match: index out of bound
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex + 1;
    t.committed = lastindex;
    t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = 0;
    t.wappend = false;
    t.wcommit = commit;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // match with the last existing entry
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = lastindex;
    //t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = lastindex;
    t.wappend = true;
    t.wcommit = lastindex;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // do not increase commit higher than lastnewi
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = lastindex + 1;
    //t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = lastindex;
    t.wappend = true;
    t.wcommit = lastindex;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // commit up to the commit in the message
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = lastindex - 1;
    //t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = lastindex;
    t.wappend = true;
    t.wcommit = lastindex - 1;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // commit do not decrease
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = 0;
    //t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = lastindex;
    t.wappend = true;
    t.wcommit = commit;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // commit do not decrease
    Test t;
    t.logTerm = 0;
    t.index = 0;
    t.committed = lastindex;
    //t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = 0;
    t.wappend = true;
    t.wcommit = commit;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // commit do not decrease
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = lastindex + 1;
    t.ents.push_back(newEntry(lastindex + 1, 4));
    t.wlasti = lastindex + 1;
    t.wappend = true;
    t.wcommit = lastindex + 1;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // do not increase commit higher than lastnewi
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = lastindex + 2;
    t.ents.push_back(newEntry(lastindex + 1, 4));
    t.wlasti = lastindex + 1;
    t.wappend = true;
    t.wcommit = lastindex + 1;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    Test t;
    t.logTerm = lastterm;
    t.index = lastindex;
    t.committed = lastindex + 2;
    t.ents.push_back(newEntry(lastindex + 1, 4));
    t.ents.push_back(newEntry(lastindex + 2, 4));
    t.wlasti = lastindex + 2;
    t.wappend = true;
    t.wcommit = lastindex + 2;
    t.wpanic = false;

    tests.push_back(t);
  }

  // match with the the entry in the middle
  {
    Test t;
    t.logTerm = lastterm - 1;
    t.index = lastindex - 1;
    t.committed = lastindex;
    t.ents.push_back(newEntry(lastindex, 4));
    t.wlasti = lastindex;
    t.wappend = true;
    t.wcommit = lastindex;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    Test t;
    t.logTerm = lastterm - 2;
    t.index = lastindex - 2;
    t.committed = lastindex;
    t.ents.push_back(newEntry(lastindex - 1, 4));
    t.wlasti = lastindex - 1;
    t.wappend = true;
    t.wcommit = lastindex - 1;
    t.wpanic = false;

    tests.push_back(t);
  }

  {
    // conflict with existing committed entry
    Test t;
    t.logTerm = lastterm - 3;
    t.index = lastindex - 3;
    t.committed = lastindex;
    t.ents.push_back(newEntry(lastindex - 2, 4));
    t.wlasti = lastindex - 2;
    t.wappend = true;
    t.wcommit = lastindex - 2;
    t.wpanic = true;

    tests.push_back(t);
  }

  {
    // conflict with existing committed entry
    Test t;
    t.logTerm = lastterm - 2;
    t.index = lastindex - 2;
    t.committed = lastindex;
    t.ents.push_back(newEntry(lastindex - 1, 4));
    t.ents.push_back(newEntry(lastindex, 4));
    t.wlasti = lastindex;
    t.wappend = true;
    t.wcommit = lastindex;
    t.wpanic = false;

    tests.push_back(t);
  }

  for (size_t i = 0; i < tests.size(); ++i) {
    MemoryStoragePtr storage(new MemoryStorage());
    RaftLog l(storage, std::numeric_limits<uint64_t>::max());
    l.append(previousEnts);
    l.committed_ = commit;

    auto& test = tests[i];

    LOG_INFO("testing maybeAppend %lu", i);
    uint64_t last;
    bool ok;

    if (test.wpanic) {
      ASSERT_ANY_THROW(l.maybe_append(test.index, test.logTerm, test.committed, test.ents, last, ok));
    } else {
      l.maybe_append(test.index, test.logTerm, test.committed, test.ents, last, ok);

      ASSERT_TRUE(last == test.wlasti);
      ASSERT_TRUE(ok == test.wappend);
      ASSERT_TRUE(test.wcommit == l.committed_);

      if (ok && !test.ents.empty()) {
        std::vector<proto::EntryPtr> ents;
        l.slice(l.last_index() - test.ents.size() + 1,
                l.last_index() + 1,
                std::numeric_limits<uint64_t>::max(),
                ents);
        ASSERT_TRUE(entry_cmp(ents, test.ents));
      }
    }
  }
}

TEST(raftlog, CompactionSideEffects) {
  uint64_t lastIndex = 1000;
  uint64_t unstableIndex = 750;
  uint64_t lastTerm = lastIndex;

  MemoryStoragePtr storage(new MemoryStorage());

  uint64_t i;
  for (i = 1; i <= unstableIndex; i++) {
    std::vector<proto::EntryPtr> entries;
    entries.push_back(newEntry(i, i));
    storage->append(std::move(entries));
  }

  RaftLog l(storage, RaftLog::unlimited());
  for (i = unstableIndex; i < lastIndex; i++) {
    std::vector<proto::EntryPtr> entries;
    entries.push_back(newEntry(i + 1, i + 1));
    l.append(std::move(entries));
  }

  bool ok = l.maybe_commit(lastIndex, lastTerm);
  ASSERT_TRUE(ok);

  l.applied_to(l.committed_);

  uint64_t offset = 500;
  Status status = storage->compact(offset);
  ASSERT_TRUE(status.is_ok());

  ASSERT_TRUE(l.last_index() == lastIndex);

  for (uint64_t j = offset; j <= l.last_index(); j++) {
    uint64_t t;
    l.term(j, t);
    ASSERT_TRUE(j == t);
  }

  for (uint64_t j = offset; j <= l.last_index(); j++) {
    ASSERT_TRUE(l.match_term(j, j));
  }

  auto unstableEnts = l.unstable_entries();
  ASSERT_TRUE(unstableEnts.size() == 250);

  ASSERT_TRUE(unstableEnts[0]->index == 751);

  uint64_t prev = l.last_index();

  {
    std::vector<proto::EntryPtr> entries;
    entries.push_back(newEntry(l.last_index() + 1, l.last_index() + 1));
    l.append(entries);
  }
  ASSERT_TRUE(l.last_index() == prev + 1);

  std::vector<proto::EntryPtr> entries;
  status = l.entries(l.last_index(), RaftLog::unlimited(), entries);
  ASSERT_TRUE(status.is_ok());
  ASSERT_TRUE(entries.size() == 1);
}

TEST(raftlog, HasNextEnts) {
  proto::SnapshotPtr snapshot(new proto::Snapshot());
  snapshot->metadata.term = 1;
  snapshot->metadata.index = 3;

  std::vector<proto::EntryPtr> entries;
  entries.push_back(newEntry(4, 1));
  entries.push_back(newEntry(5, 1));
  entries.push_back(newEntry(6, 1));

  struct Test {
    uint64_t applied;
    bool hasNext;
  };

  std::vector<Test> tests;
  tests.push_back(Test{.applied = 0, .hasNext = true});
  tests.push_back(Test{.applied = 3, .hasNext = true});
  tests.push_back(Test{.applied = 4, .hasNext = true});
  tests.push_back(Test{.applied = 5, .hasNext = false});

  for (size_t i = 0; i < tests.size(); ++i) {
    LOG_INFO("testing %lu", i);
    MemoryStoragePtr storage(new MemoryStorage());
    storage->apply_snapshot(*snapshot);

    RaftLog l(storage, RaftLog::unlimited());

    l.append(entries);
    l.maybe_commit(5, 1);
    l.applied_to(tests[i].applied);

    ASSERT_TRUE(l.has_next_entries() == tests[i].hasNext);
  }
}

TEST(raftlog, NextEnts) {
  proto::SnapshotPtr snapshot(new proto::Snapshot());
  snapshot->metadata.term = 1;
  snapshot->metadata.index = 3;

  std::vector<proto::EntryPtr> entries;
  entries.push_back(newEntry(4, 1));
  entries.push_back(newEntry(5, 1));
  entries.push_back(newEntry(6, 1));

  struct Test {
    uint64_t applied;
    std::vector<proto::EntryPtr> wents;
  };

  std::vector<Test> tests;
  {
    size_t low = 0;
    size_t high = 2;
    std::vector<proto::EntryPtr> wents;
    for (size_t i = low; i < high; ++i) {
      wents.push_back(entries[i]);
    }

    tests.push_back(Test{.applied = 0, .wents = wents});
  }

  {
    size_t low = 0;
    size_t high = 2;
    std::vector<proto::EntryPtr> wents;
    for (size_t i = low; i < high; ++i) {
      wents.push_back(entries[i]);
    }

    tests.push_back(Test{.applied = 3, .wents = wents});
  }

  {
    size_t low = 1;
    size_t high = 2;
    std::vector<proto::EntryPtr> wents;
    for (size_t i = low; i < high; ++i) {
      wents.push_back(entries[i]);
    }

    tests.push_back(Test{.applied = 4, .wents = wents});
  }

  {
    std::vector<proto::EntryPtr> wents;
    tests.push_back(Test{.applied = 5, .wents = wents});
  }

  for (size_t i = 0; i < tests.size(); ++i) {
    LOG_INFO("testing NextEnts %lu", i);
    MemoryStoragePtr storage(new MemoryStorage());
    storage->apply_snapshot(*snapshot);

    RaftLog l(storage, RaftLog::unlimited());

    l.append(entries);
    ASSERT_TRUE(l.maybe_commit(5, 1));
    l.applied_to(tests[i].applied);

    std::vector<proto::EntryPtr> list;
    l.next_entries(list);
    ASSERT_TRUE(entry_cmp(list, tests[i].wents));
  }
}

TEST(raftlog, UnstableEnts) {
  std::vector<proto::EntryPtr> previousEnts;
  previousEnts.push_back(newEntry(1, 1));
  previousEnts.push_back(newEntry(2, 2));

  struct Test {
    uint64_t unstable;
    std::vector<proto::EntryPtr> wentries;
  };

  std::vector<Test> tests;
  tests.push_back(Test{.unstable = 3,});
  tests.push_back(Test{.unstable = 1, previousEnts});

  for (size_t i = 0; i < tests.size(); ++i) {
    LOG_INFO("testing UnstableEnts %lu", i);
    Test& test = tests[i];

    MemoryStoragePtr storage(new MemoryStorage());
    std::vector<proto::EntryPtr> entries;

    for (uint64_t j = 0; j < test.unstable - 1; ++j) {
      entries.push_back(previousEnts[j]);
    }
    storage->append(entries);

    RaftLog l(storage, RaftLog::unlimited());

    entries.clear();
    for (uint64_t j = test.unstable - 1; j < previousEnts.size(); ++j) {
      entries.push_back(previousEnts[j]);
    }

    l.append(entries);

    std::vector<proto::EntryPtr> ents;
    auto out = l.unstable_entries();
    auto len = out.size();
    if (!out.empty()) {
      l.stable_to(out[len - 1]->index, out[len - i]->term);
    }
    ASSERT_TRUE(entry_cmp(out, test.wentries));

    auto w = previousEnts.back()->index + 1;
    ASSERT_TRUE(l.unstable_->offset_ == w);
  }
}

TEST(raftlog, committo) {
  std::vector<proto::EntryPtr> previousEnts;
  previousEnts.push_back(newEntry(1, 1));
  previousEnts.push_back(newEntry(2, 2));
  previousEnts.push_back(newEntry(3, 3));

  struct Test {
    uint64_t commit;
    uint64_t wcommit;
    bool wpanic;
  };

  std::vector<Test> tests;

  tests.push_back(Test{.commit = 3, .wcommit= 3, .wpanic= false});
  tests.push_back(Test{.commit = 1, .wcommit= 2, .wpanic= false}); // never decrease
  tests.push_back(Test{.commit = 4, .wcommit= 0, .wpanic= true}); // commit out of range -> panic

  for (size_t i = 0; i < tests.size(); ++i) {
    LOG_INFO("testing committo %lu", i);
    Test& test = tests[i];

    MemoryStoragePtr storage(new MemoryStorage());
    RaftLog l(storage, std::numeric_limits<uint64_t>::max());

    l.append(previousEnts);

    l.committed_ = 2;

    if (test.wpanic) {
      ASSERT_ANY_THROW(l.commit_to(test.commit));

    } else {
      l.commit_to(test.commit);
      ASSERT_TRUE(test.wcommit == l.committed_);
    }
  }
}

TEST(raftlog, stableto) {
  struct Test {
    uint64_t stablei;
    uint64_t stablet;
    uint64_t wunstable;
  };

  std::vector<Test> tests;
  tests.push_back(Test{.stablei = 1, .stablet = 1, .wunstable = 2});
  tests.push_back(Test{.stablei = 2, .stablet = 2, .wunstable = 3});
  tests.push_back(Test{.stablei = 2, .stablet = 1, .wunstable = 1}); // bad term
  tests.push_back(Test{.stablei = 3, .stablet = 1, .wunstable = 1});  // bad index

  for (size_t i = 0; i < tests.size(); ++i) {
    MemoryStoragePtr storage(new MemoryStorage());
    RaftLog l(storage, std::numeric_limits<uint64_t>::max());

    std::vector<proto::EntryPtr> previousEnts;
    previousEnts.push_back(newEntry(1, 1));
    previousEnts.push_back(newEntry(2, 2));

    l.append(previousEnts);

    Test& tt = tests[i];
    l.stable_to(tt.stablei, tt.stablet);

    ASSERT_TRUE(tt.wunstable == l.unstable_->offset_);

  }
}

TEST(raftlog, stabletosnap) {
  uint64_t snapi = 5;
  uint64_t snapt = 2;
  struct Test {
    uint64_t stablei;
    uint64_t stablet;
    std::vector<proto::EntryPtr> newEnts;
    uint64_t wunstable;
  };

  std::vector<Test> tests;
  {
    std::vector<proto::EntryPtr> ents;
    tests.push_back(Test{.stablei = snapi + 1, .stablet = snapt, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    tests.push_back(Test{.stablei = snapi, .stablet = snapt, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    tests.push_back(Test{.stablei = snapi - 1, .stablet = snapt, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    tests.push_back(Test{.stablei = snapi + 1, .stablet = snapt + 1, .newEnts = ents, .wunstable = snapi + 1});
  }

  {
    std::vector<proto::EntryPtr> ents;
    tests.push_back(Test{.stablei = snapi, .stablet = snapt + 1, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    tests.push_back(Test{.stablei = snapi - 1, .stablet = snapt + 1, .newEnts = ents, .wunstable = snapi + 1});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(snapi + 1, snapt));
    tests.push_back(Test{.stablei = snapi + 1, .stablet = snapt, .newEnts = ents, .wunstable = snapi + 2});
  }
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(snapi + 1, snapt));
    tests.push_back(Test{.stablei = snapi, .stablet = snapt, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(snapi + 1, snapt));
    tests.push_back(Test{.stablei = snapi - 1, .stablet = snapt, .newEnts = ents, .wunstable = snapi + 1});
  }

  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(snapi + 1, snapt));
    tests.push_back(Test{.stablei = snapi + 1, .stablet = snapt + 1, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(snapi + 1, snapt));
    tests.push_back(Test{.stablei = snapi, .stablet = snapt + 1, .newEnts = ents, .wunstable = snapi + 1});
  }
  {
    std::vector<proto::EntryPtr> ents;
    ents.push_back(newEntry(snapi + 1, snapt));
    tests.push_back(Test{.stablei = snapi - 1, .stablet = snapt + 1, .newEnts = ents, .wunstable = snapi + 1});
  }

  for (size_t i = 0; i < tests.size(); ++i) {
    MemoryStoragePtr storage(new MemoryStorage());
    proto::SnapshotPtr snapshot(new proto::Snapshot());
    snapshot->metadata.term = snapt;
    snapshot->metadata.index = snapi;
    storage->apply_snapshot(*snapshot);

    RaftLog l(storage, std::numeric_limits<uint64_t>::max());

    Test& tt = tests[i];
    l.append(tt.newEnts);

    l.stable_to(tt.stablei, tt.stablet);

    ASSERT_TRUE(tt.wunstable == l.unstable_->offset_);

  }
}

//TestCompaction ensures that the number of log entries is correct after compactions.
TEST(raftlog, Compaction) {
  struct Test {
    uint64_t lastIndex;
    std::vector<uint64_t> compact;
    std::vector<int> wleft;
    bool wallow;
  };

  std::vector<Test> tests;

  for (size_t i = 0; i < tests.size(); ++i) {
    Test& test = tests[i];
    std::vector<proto::EntryPtr> ents;
  }

  //TODO

}

TEST(raftlog, LogRestore) {
  //todo

}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
