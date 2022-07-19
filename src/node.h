#ifndef NODE_TREE_SITTER_NODE_H_
#define NODE_TREE_SITTER_NODE_H_

#include <napi.h>
#include <tree_sitter/api.h>
#include "./tree.h"
#include "./util.h"

namespace node_tree_sitter {

using namespace Napi;

void InitNode(Napi::Env env, Napi::Object &exports);
Napi::Value MarshalNode(Napi::Env, const Tree *, TSNode);
Napi::Value MarshalNodes(Napi::Env env, const Tree *tree, const TSNode *nodes, uint32_t node_count);
TSNode UnmarshalNode(Napi::Env env, const Tree *tree);

class NodeMethods : public Napi::ObjectWrap<NodeMethods> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object &exports);
  NodeMethods(const Napi::CallbackInfo &info);

  static Napi::Value SetNodeTransferArray(const Napi::CallbackInfo &);
  static Napi::Value NodeTransferArray(const Napi::CallbackInfo &);

  static Napi::Value StartIndex(const Napi::CallbackInfo &);
  static Napi::Value EndIndex(const Napi::CallbackInfo &);
  static Napi::Value Type(const Napi::CallbackInfo &);
  static Napi::Value TypeId(const Napi::CallbackInfo &);
  static Napi::Value IsNamed(const Napi::CallbackInfo &);
  static Napi::Value Parent(const Napi::CallbackInfo &);
  static Napi::Value Child(const Napi::CallbackInfo &);
  static Napi::Value NamedChild(const Napi::CallbackInfo &);
  static Napi::Value Children(const Napi::CallbackInfo &);
  static Napi::Value NamedChildren(const Napi::CallbackInfo &);
  static Napi::Value ChildCount(const Napi::CallbackInfo &);
  static Napi::Value NamedChildCount(const Napi::CallbackInfo &);
  static Napi::Value FirstChild(const Napi::CallbackInfo &);
  static Napi::Value LastChild(const Napi::CallbackInfo &);
  static Napi::Value FirstNamedChild(const Napi::CallbackInfo &);
  static Napi::Value LastNamedChild(const Napi::CallbackInfo &);
  static Napi::Value NextSibling(const Napi::CallbackInfo &);
  static Napi::Value NextNamedSibling(const Napi::CallbackInfo &);
  static Napi::Value PreviousSibling(const Napi::CallbackInfo &);
  static Napi::Value PreviousNamedSibling(const Napi::CallbackInfo &);
  static Napi::Value StartPosition(const Napi::CallbackInfo &);
  static Napi::Value EndPosition(const Napi::CallbackInfo &);
  static Napi::Value IsMissing(const Napi::CallbackInfo &);
  static Napi::Value ToString(const Napi::CallbackInfo &);
  static Napi::Value FirstChildForIndex(const Napi::CallbackInfo &);
  static Napi::Value FirstNamedChildForIndex(const Napi::CallbackInfo &);
  static Napi::Value DescendantForIndex(const Napi::CallbackInfo &);
  static Napi::Value NamedDescendantForIndex(const Napi::CallbackInfo &);
  static Napi::Value DescendantForPosition(const Napi::CallbackInfo &);
  static Napi::Value NamedDescendantForPosition(const Napi::CallbackInfo &);
  static Napi::Value HasChanges(const Napi::CallbackInfo &);
  static Napi::Value HasError(const Napi::CallbackInfo &);
  static Napi::Value DescendantsOfType(const Napi::CallbackInfo &);
  static Napi::Value Walk(const Napi::CallbackInfo &);
  static Napi::Value Closest(const Napi::CallbackInfo &);
  static Napi::Value ChildNodeForFieldId(const Napi::CallbackInfo &);
  static Napi::Value ChildNodesForFieldId(const Napi::CallbackInfo &);
 private:
  static Napi::FunctionReference * constructor;
};

}  // namespace node_tree_sitter

#endif  // NODE_TREE_SITTER_NODE_H_
