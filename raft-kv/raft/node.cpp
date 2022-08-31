#include <raft-kv/raft/node.h>
#include <raft-kv/common/log.h>

namespace kv {

Node* Node::start_node(const Config& conf, const std::vector<PeerContext>& peers) {
  return new RawNode(conf, peers);
}

Node* Node::restart_node(const Config& conf) {
  return new RawNode(conf);
}

RawNode::RawNode(const Config& conf, const std::vector<PeerContext>& peers) {
  raft_ = std::make_shared<Raft>(conf);

  uint64_t last_index = 0;
  Status status = conf.storage->last_index(last_index);
  if (!status.is_ok()) {
    LOG_FATAL("%s", status.to_string().c_str());
  }

  // If the log is empty, this is a new RawNode (like StartNode); otherwise it's
  // restoring an existing RawNode (like RestartNode).
  // 如果 lastindex = 0 说明该节点还没有被初始化，并且他是一个 startNode
  if (last_index == 0) {
    raft_->become_follower(1, 0);

    std::vector<proto::EntryPtr> entries;
    // 将每个节点信息保存在该节点的 entry 中
    for (size_t i = 0; i < peers.size(); ++i) {
      auto& peer = peers[i];
      // 节点发生变化的状态机
      proto::ConfChange cs = proto::ConfChange{
          .id = 0,
          .conf_change_type = proto::ConfChangeAddNode,
          .node_id = peer.id,
          .context = peer.context,
      };

      std::vector<uint8_t> data = cs.serialize(); // 将 node 的信息序列化保存为一个二进制

      proto::EntryPtr entry(new proto::Entry());
      entry->type = proto::EntryConfChange; // 集群节点状态发生变化
      entry->term = 1; // 一开始没有选举的时候 term 为 0 后面成为 follower 紧接着变为 1 
      entry->index = i + 1;
      entry->data = std::move(data);
      entries.push_back(entry);
    }

    raft_->raft_log_->append(entries);
    raft_->raft_log_->committed_ = entries.size();

    for (auto& peer : peers) {
      raft_->add_node(peer.id);
    }
  }

  // Set the initial hard and soft states after performing all initialization.
  prev_soft_state_ = raft_->soft_state();
  if (last_index == 0) {
    prev_hard_state_ = proto::HardState();
  } else {
    prev_hard_state_ = raft_->hard_state();
  }
}

RawNode::RawNode(const Config& conf) {

  uint64_t last_index = 0;
  Status status = conf.storage->last_index(last_index);
  if (!status.is_ok()) {
    LOG_FATAL("%s", status.to_string().c_str());
  }

  raft_ = std::make_shared<Raft>(conf);

  // Set the initial hard and soft states after performing all initialization.
  prev_soft_state_ = raft_->soft_state();
  if (last_index == 0) {
    prev_hard_state_ = proto::HardState();
  } else {
    prev_hard_state_ = raft_->hard_state();
  }
}


void RawNode::tick() {
  raft_->tick();
}

Status RawNode::campaign() {
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgHup;
  return raft_->step(std::move(msg));
}

Status RawNode::propose(std::vector<uint8_t> data) {
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgProp;
  msg->from = raft_->id_;
  msg->entries.emplace_back(proto::EntryNormal, 0, 0, std::move(data)); // 先将消息添加到 entries 中

  return raft_->step(std::move(msg));
}

Status RawNode::propose_conf_change(const proto::ConfChange& cc) {
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgProp;
  msg->entries.emplace_back(proto::EntryConfChange, 0, 0, cc.serialize());
  return raft_->step(std::move(msg));
}

Status RawNode::step(proto::MessagePtr msg) {
  // ignore unexpected local messages receiving over network
  if (msg->is_local_msg()) {
    return Status::invalid_argument("raft: cannot step raft local message");
  }

  ProgressPtr progress = raft_->get_progress(msg->from);
  if (progress || !msg->is_response_msg()) {
    return raft_->step(msg);
  }
  return Status::invalid_argument("raft: cannot step as peer not found");
}

ReadyPtr RawNode::ready() {
  ReadyPtr rd = std::make_shared<Ready>(raft_, prev_soft_state_, prev_hard_state_);
  raft_->msgs_.clear();
  raft_->reduce_uncommitted_size(rd->committed_entries);
  return rd;
}

bool RawNode::has_ready() {
  assert(prev_soft_state_);
  if (!raft_->soft_state()->equal(*prev_soft_state_)) {
    return true;
  }
  proto::HardState hs = raft_->hard_state();
  if (!hs.is_empty_state() && !hs.equal(prev_hard_state_)) {
    return true;
  }

  proto::SnapshotPtr snapshot = raft_->raft_log_->unstable_->snapshot_;

  if (snapshot && !snapshot->is_empty()) {
    return true;
  }
  if (!raft_->msgs_.empty() || !raft_->raft_log_->unstable_entries().empty()
      || raft_->raft_log_->has_next_entries()) {
    return true;
  }

  return !raft_->read_states_.empty();
}

void RawNode::advance(ReadyPtr rd) {
  if (rd->soft_state) {
    prev_soft_state_ = rd->soft_state;

  }
  if (!rd->hard_state.is_empty_state()) {
    prev_hard_state_ = rd->hard_state;
  }

  // If entries were applied (or a snapshot), update our cursor for
  // the next Ready. Note that if the current HardState contains a
  // new Commit index, this does not mean that we're also applying
  // all of the new entries due to commit pagination by size.
  uint64_t index = rd->applied_cursor();
  if (index > 0) {
    raft_->raft_log_->applied_to(index);
  }

  if (!rd->entries.empty()) {
    auto& entry = rd->entries.back();
    raft_->raft_log_->stable_to(entry->index, entry->term);
  }

  if (!rd->snapshot.is_empty()) {
    raft_->raft_log_->stable_snap_to(rd->snapshot.metadata.index);
  }

  if (!rd->read_states.empty()) {
    raft_->read_states_.clear();
  }
}

proto::ConfStatePtr RawNode::apply_conf_change(const proto::ConfChange& cc) {
  proto::ConfStatePtr state(new proto::ConfState());
  if (cc.node_id == 0) {
    raft_->nodes(state->nodes);
    raft_->learner_nodes(state->learners);
    return state;
  }

  switch (cc.conf_change_type) {
    case proto::ConfChangeAddNode: {
      raft_->add_node_or_learner(cc.node_id, false);
      break;
    }
    case proto::ConfChangeAddLearnerNode: {
      raft_->add_node_or_learner(cc.node_id, true);
      break;
    }
    case proto::ConfChangeRemoveNode: {
      raft_->remove_node(cc.node_id);
      break;
    }
    case proto::ConfChangeUpdateNode: {
      LOG_DEBUG("ConfChangeUpdate");
      break;
    }
    default: {
      LOG_FATAL("unexpected conf type");
    }
  }
  raft_->nodes(state->nodes);
  raft_->learner_nodes(state->learners);
  return state;
}

void RawNode::transfer_leadership(uint64_t lead, ino64_t transferee) {
  // manually set 'from' and 'to', so that leader can voluntarily transfers its leadership
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgTransferLeader;
  msg->from = transferee;
  msg->to = lead;

  Status status = raft_->step(std::move(msg));
  if (!status.is_ok()) {
    LOG_WARN("transfer_leadership %s", status.to_string().c_str());
  }
}

Status RawNode::read_index(std::vector<uint8_t> rctx) {
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgReadIndex;
  msg->entries.emplace_back(proto::MsgReadIndex, 0, 0, std::move(rctx));
  return raft_->step(std::move(msg));
}

RaftStatusPtr RawNode::raft_status() {
  LOG_DEBUG("no impl yet");
  return nullptr;
}

void RawNode::report_unreachable(uint64_t id) {
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgUnreachable;
  msg->from = id;

  Status status = raft_->step(std::move(msg));
  if (!status.is_ok()) {
    LOG_WARN("report_unreachable %s", status.to_string().c_str());
  }
}

void RawNode::report_snapshot(uint64_t id, SnapshotStatus status) {
  bool rej = (status == SnapshotFailure);
  proto::MessagePtr msg(new proto::Message());
  msg->type = proto::MsgSnapStatus;
  msg->from = id;
  msg->reject = rej;

  Status s = raft_->step(std::move(msg));
  if (!s.is_ok()) {
    LOG_WARN("report_snapshot %s", s.to_string().c_str());
  }
}

void RawNode::stop() {

}

}