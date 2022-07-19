#ifndef NODE_TREE_SITTER_TREE_H_
#define NODE_TREE_SITTER_TREE_H_

#include <napi.h>
#include <unordered_map>
#include <tree_sitter/api.h>

namespace node_tree_sitter {

class Tree : public Napi::ObjectWrap<Tree> {
 public:
  static void Init(Napi::Object &);
  static const Tree *UnwrapTree(const Napi::Value &);
  Tree(const Napi::CallbackInfo& info);
  ~Tree();

  Napi::Value New(const Napi::CallbackInfo &);
  Napi::Value Edit(const Napi::CallbackInfo &);
  Napi::Value RootNode(const Napi::CallbackInfo &);
  Napi::Value PrintDotGraph(const Napi::CallbackInfo &);
  Napi::Value GetEditedRange(const Napi::CallbackInfo &);
  Napi::Value GetChangedRanges(const Napi::CallbackInfo &);
  Napi::Value CacheNode(const Napi::CallbackInfo &);
  Napi::Value CacheNodes(const Napi::CallbackInfo &);

  static Napi::FunctionReference * constructor;

  struct NodeCacheEntry {
    Tree *tree;
    const void *key;
    Napi::ObjectReference node;
  };

  TSTree *tree_;
  std::unordered_map<const void *, NodeCacheEntry *> cached_nodes_;
private:
  Napi::Value CacheNodeForTree(const Napi::Object &);
};

}  // namespace node_tree_sitter

#endif  // NODE_TREE_SITTER_TREE_H_
