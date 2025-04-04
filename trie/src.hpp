/*
 * Acknowledgement : Yuxuan Wang for Modifying the prototype of TrieStore class
 */

#ifndef SJTU_TRIE_HPP
#define SJTU_TRIE_HPP

#include <algorithm>
#include <cstddef>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sjtu {

// A TrieNode is a node in a Trie.
class TrieNode {
public:
  // Create a TrieNode with no children.
  TrieNode() = default;

  // Create a TrieNode with some children.
  explicit TrieNode(std::map<char, std::shared_ptr<const TrieNode>> children)
      : children_(std::move(children)) {}

  virtual ~TrieNode() = default;

  // Clone returns a copy of this TrieNode. If the TrieNode has a value, the
  // value is copied. The return type of this function is a unique_ptr to a
  // TrieNode.
  //
  // You cannot use the copy constructor to clone the node because it doesn't
  // know whether a `TrieNode` contains a value or not.
  //
  // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use
  // `std::shared_ptr<T>(std::move(ptr))`.
  virtual auto Clone() const -> std::unique_ptr<TrieNode> {
    return std::make_unique<TrieNode>(children_);
  }

  // A map of children, where the key is the next character in the key, and
  // the value is the next TrieNode.
  std::map<char, std::shared_ptr<const TrieNode>> children_;

  // Indicates if the node is the terminal node.
  bool is_value_node_{false};

  // You can add additional fields and methods here. But in general, you don't
  // need to add extra fields to complete this project.
};

// A TrieNodeWithValue is a TrieNode that also has a value of type T associated
// with it.
template <class T> class TrieNodeWithValue : public TrieNode {
public:
  // Create a trie node with no children and a value.
  explicit TrieNodeWithValue(std::shared_ptr<T> value)
      : value_(std::move(value)) {
    this->is_value_node_ = true;
  }

  // Create a trie node with children and a value.
  TrieNodeWithValue(std::map<char, std::shared_ptr<const TrieNode>> children,
                    std::shared_ptr<T> value)
      : TrieNode(std::move(children)), value_(std::move(value)) {
    this->is_value_node_ = true;
  }

  // Override the Clone method to also clone the value.
  //
  // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use
  // `std::shared_ptr<T>(std::move(ptr))`.
  auto Clone() const -> std::unique_ptr<TrieNode> override {
    return std::make_unique<TrieNodeWithValue<T>>(children_, value_);
  }

  // The value associated with this trie node.
  std::shared_ptr<T> value_;
};

// A Trie is a data structure that maps strings to values of type T. All
// operations on a Trie should not modify the trie itself. It should reuse the
// existing nodes as much as possible, and create new nodes to represent the new
// trie.
class Trie {
private:
  // The root of the trie.
  std::shared_ptr<const TrieNode> root_{nullptr};

  // Create a new trie with the given root.
  explicit Trie(std::shared_ptr<const TrieNode> root)
      : root_(std::move(root)) {}

public:
  // Create an empty trie.
  Trie() = default;

  bool operator==(const Trie &other) const { return root_ == other.root_; };
  //  by TA: if you don't need this, just comment out.

  // Get the value associated with the given key.
  // 1. If the key is not in the trie, return nullptr.
  // 2. If the key is in the trie but the type is mismatched, return nullptr.
  // 3. Otherwise, return the value.
  template <class T> auto Get(std::string_view key) const -> const T * {
    std::shared_ptr<const TrieNode> cur = root_;

    for (auto ch : key) {
      if (!cur)
        return nullptr;

      if (cur->children_.find(ch) == cur->children_.end())
        return nullptr;

      cur = cur->children_.at(ch);
    }

    if (cur && cur->is_value_node_) {
      auto node = std::dynamic_pointer_cast<const TrieNodeWithValue<T>>(cur);
      if (node)
        return node->value_.get();
    }

    return nullptr;
  };

  // Put a new key-value pair into the trie. If the key already exists,
  // overwrite the value. Returns the new trie.
  template <class T> auto Put(std::string_view key, T value) const -> Trie {
    std::shared_ptr<TrieNode> new_root =
        root_ ? root_->Clone() : std::make_shared<TrieNode>();
    std::shared_ptr<TrieNode> parent = new_root, pre = nullptr;
    char pre_char = 0;
    for (char ch : key) {
      pre = parent;
      pre_char = ch;
      // 复制当前路径上的节点

      std::shared_ptr<TrieNode> new_node;

      auto it = parent->children_.find(ch);
      new_node = (it != parent->children_.end()) ? it->second->Clone()
                                                 : std::make_shared<TrieNode>();

      parent->children_[ch] = new_node;
      parent = new_node;
    }
    if (!parent->is_value_node_) {
      auto valuePtr = std::make_shared<T>(std::move(value));
      auto new_node = std::make_shared<TrieNodeWithValue<T>>(
          parent->children_, std::move(valuePtr));
      pre->children_[pre_char] = std::move(new_node);
    } else {
      auto node = std::dynamic_pointer_cast<TrieNodeWithValue<T>>(parent);
      if (node) {
        node->value_ = std::make_shared<T>(std::move(value));
      }
    }

    return Trie(new_root);
  };

  // Remove the key from the trie. If the key does not exist, return the
  // original trie. Otherwise, returns the new trie.
  auto Remove(std::string_view key) const -> Trie {
    if (!root_)
      return *this;

    std::shared_ptr<const TrieNode> cur = root_;
    std::shared_ptr<TrieNode> new_root = cur->Clone();
    std::shared_ptr<TrieNode> parent = new_root;

    std::vector<std::pair<std::shared_ptr<TrieNode>, char>>
        path; // 记录访问路径

    for (auto ch : key) {
      if (!parent || parent->children_.find(ch) == parent->children_.end()) {
        return *this;
      }

      std::shared_ptr<TrieNode> new_node =
          parent->children_.find(ch)->second->Clone();

      parent->children_[ch] = new_node;
      path.emplace_back(parent, ch);
      parent = new_node;
    }

    if (parent->is_value_node_) {
      parent->is_value_node_ = false; // 取消标记

      if (parent->children_.empty()) {
        // 如果当前是叶子节点，直接回溯删除
        path.pop_back();
      } else {
        // 替换成普通 TrieNode，去掉值信息
        parent = std::make_shared<TrieNode>(parent->children_);
        if (!path.empty()) {
          auto [prev, prev_char] = path.back();
          prev->children_[prev_char] = parent;
        } else {
          new_root = parent; // 处理根节点情况
        }
      }
    } else {
      return *this; // 该key没有值，返回原Trie
    }

    while (!path.empty()) {
      auto [node, ch] = path.back();
      path.pop_back();

      auto it = node->children_.find(ch);
      if (it == node->children_.end())
        continue;

      std::shared_ptr<TrieNode> child =
          std::const_pointer_cast<TrieNode>(it->second);
      if (!child->is_value_node_ && child->children_.empty()) {
        node->children_.erase(ch); // 删除无效子节点
      } else {
        break; // 仍有子节点或存值，不再向上删除
      }
    }

    return Trie(new_root);
  };
};

// This class is used to guard the value returned by the trie. It holds a
// reference to the root so that the reference to the value will not be
// invalidated.
template <class T> class ValueGuard {
public:
  ValueGuard(Trie root, const T &value)
      : root_(std::move(root)), value_(value) {}
  auto operator*() const -> const T & { return value_; }

private:
  Trie root_;
  const T &value_;
};

// This class is a thread-safe wrapper around the Trie class. It provides a
// simple interface for accessing the trie. It should allow concurrent reads and
// a single write operation at the same time.

// Note:
// 实际上为什么要有两个互斥量，write_mutex是为了防止Put和Remove并发，snapshot_mutex是为了防止Get方法里面
// 出现snapshot（vector）的失效。同样的道理，在Put和Remove里面涉及到对snapshot的读和写都可以造成影响，因此要用一个
// shared_lock进行保护。
// 目前的认知大致如此，后续要更深入解释就要研究一下底层了。

class TrieStore {
public:
  // This function returns a ValueGuard object that holds a reference to the
  // value in the trie of the given version (default: newest version). If the
  // key does not exist in the trie, it will return std::nullopt.
  template <class T>
  auto Get(std::string_view key, size_t version = -1)
      -> std::optional<ValueGuard<T>> {
    // std::shared_lock<std::shared_mutex> r_lock(snapshots_lock_);
    std::shared_lock<std::shared_mutex> lock(snapshots_lock_);

    if (version == -1)
      version = snapshots_.size() - 1;

    if (version >= snapshots_.size()) {
      return std::nullopt; // 版本号无效
    }

    const T *res = snapshots_[version].Get<T>(key);
    if (res)
      return ValueGuard<T>(snapshots_[version], *res);

    return std::nullopt;
  };

  // This function will insert the key-value pair into the trie. If the key
  // already exists in the trie, it will overwrite the value return the
  // version number after operation Hint: new version should only be visible
  // after the operation is committed(completed)
  template <class T> size_t Put(std::string_view key, T value) {
    std::lock_guard<std::mutex> w_lock(write_lock_);

    Trie old_trie;
    {
      std::shared_lock<std::shared_mutex> read_lock(snapshots_lock_);
      old_trie = snapshots_.back();
    }
    Trie new_trie = old_trie.Put(key, std::move(value));
    {
      std::unique_lock<std::shared_mutex> write_lock(snapshots_lock_);
      snapshots_.push_back(std::move(new_trie));
    }

    return get_version();
  };

  // This function will remove the key-value pair from the trie.
  // return the version number after operation
  // if the key does not exist, version number should not be increased
  size_t Remove(std::string_view key) {
    std::lock_guard<std::mutex> w_lock(write_lock_);

    Trie new_trie;

    {
      std::shared_lock<std::shared_mutex> read_lock(snapshots_lock_);
      new_trie = snapshots_.back().Remove(key);
    }

    {
      std::unique_lock<std::shared_mutex> write_lock(snapshots_lock_);
      if (new_trie != snapshots_.back()) { // 只有 key 存在时才增加版本
        snapshots_.push_back(std::move(new_trie));
      }
    }

    return get_version();
  };

  // This function return the newest version number
  size_t get_version() const {
    return version_.load(std::memory_order_relaxed);
  };

private:
  // 读写锁，保护 snapshots_ 的读写操作
  std::shared_mutex snapshots_lock_;
  std::mutex write_lock_;
  std::atomic<size_t> version_;

  // Stores all historical versions of trie
  // version number ranges from [0, snapshots_.size())
  std::vector<Trie> snapshots_{1};
};

} // namespace sjtu

#endif // SJTU_TRIE_HPP