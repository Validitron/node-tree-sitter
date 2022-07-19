#ifndef NODE_TREE_SITTER_TREE_CURSOR_H_
#define NODE_TREE_SITTER_TREE_CURSOR_H_

#include <napi.h>
#include <tree_sitter/api.h>

namespace node_tree_sitter {

using namespace Napi;

class TreeCursor : public Napi::ObjectWrap<TreeCursor> {
 public:
   static void Init(Napi::Object &);
   static Napi::Value NewTreeCursor(TSTreeCursor);

   TreeCursor(const CallbackInfo &info);
   ~TreeCursor();

   Napi::Value GotoParent(const CallbackInfo &info);
   Napi::Value GotoFirstChild(const CallbackInfo &info);
   Napi::Value GotoFirstChildForIndex(const CallbackInfo &info);
   Napi::Value GotoNextSibling(const CallbackInfo &info);
   Napi::Value StartPosition(const CallbackInfo &info);
   Napi::Value EndPosition(const CallbackInfo &info);
   Napi::Value CurrentNode(const CallbackInfo &info);
   Napi::Value Reset(const CallbackInfo &info);
   Napi::Value NodeType(const CallbackInfo &info);
   Napi::Value NodeIsNamed(const CallbackInfo &info);
   Napi::Value CurrentFieldName(const CallbackInfo &info) ;
   Napi::Value StartIndex(const CallbackInfo &info);
   Napi::Value EndIndex(const CallbackInfo &info);

   TSTreeCursor cursor_;

   static Napi::FunctionReference* constructor;

 private:
};

}  // namespace node_tree_sitter

#endif  // NODE_TREE_SITTER_TREE_CURSOR_H_
