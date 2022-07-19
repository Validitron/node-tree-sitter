#include "./tree_cursor.h"
#include <tree_sitter/api.h>
#include <napi.h>
#include "./util.h"
#include "./conversions.h"
#include "./node.h"
#include "./tree.h"

namespace node_tree_sitter {

using namespace Napi;

Napi::FunctionReference* TreeCursor::constructor = new Napi::FunctionReference();;

void TreeCursor::Init(Napi::Object &exports) {
  Napi::Env env = exports.Env();

  Napi::Function ctor = DefineClass(env, "TreeCursor", {
    InstanceAccessor("startIndex", &TreeCursor::StartIndex, nullptr),
    InstanceAccessor("endIndex", &TreeCursor::EndIndex, nullptr),
    InstanceAccessor("nodeType", &TreeCursor::NodeType, nullptr),
    InstanceAccessor("nodeIsNamed", &TreeCursor::NodeIsNamed, nullptr),
    InstanceAccessor("currentFieldName", &TreeCursor::CurrentFieldName, nullptr),

    InstanceMethod("startPosition", &TreeCursor::StartPosition, napi_configurable),
    InstanceMethod("endPosition", &TreeCursor::EndPosition, napi_configurable),
    InstanceMethod("gotoParent", &TreeCursor::GotoParent),
    InstanceMethod("gotoFirstChild", &TreeCursor::GotoFirstChild),
    InstanceMethod("gotoFirstChildForIndex", &TreeCursor::GotoFirstChildForIndex),
    InstanceMethod("gotoNextSibling", &TreeCursor::GotoNextSibling),
    InstanceMethod("currentNode", &TreeCursor::CurrentNode, napi_configurable),
    InstanceMethod("reset", &TreeCursor::Reset),
  });

  *constructor = Napi::Persistent(ctor);

  exports.Set("TreeCursor", ctor);
}

TreeCursor::TreeCursor(const Napi::CallbackInfo &info)
  : Napi::ObjectWrap<TreeCursor>(info),
    cursor_({0, 0, {0, 0}})
    {}

TreeCursor::~TreeCursor() {
  ts_tree_cursor_delete(&cursor_);
}

Napi::Value TreeCursor::GotoParent(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  bool result = ts_tree_cursor_goto_parent(&cursor_);
  return Napi::Boolean::New(env, result);
}

Napi::Value TreeCursor::GotoFirstChild(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  bool result = ts_tree_cursor_goto_first_child(&cursor_);
  return Napi::Boolean::New(env, result);
}

Napi::Value TreeCursor::GotoFirstChildForIndex(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  auto js_index = info[0].As<Napi::Number>();
  if (!js_index.IsNumber()) {
    TypeError::New(env, "Argument must be an integer").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  uint32_t goal_byte = js_index.Uint32Value() * 2;
  int64_t child_index = ts_tree_cursor_goto_first_child_for_byte(&cursor_, goal_byte);
  if (child_index < 0) {
    return env.Null();
  } else {
    return Napi::Number::New(env, child_index);
  }
}

Napi::Value TreeCursor::GotoNextSibling(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  bool result = ts_tree_cursor_goto_next_sibling(&cursor_);
  return Napi::Boolean::New(env, result);
}

Napi::Value TreeCursor::StartPosition(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  TransferPoint(ts_node_start_point(node));
  return env.Undefined();
}

Napi::Value TreeCursor::EndPosition(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  TransferPoint(ts_node_end_point(node));
  return env.Undefined();
}

Napi::Value TreeCursor::CurrentNode(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  Napi::Value js_tree = info.This().As<Napi::Object>()["tree"];
  const Tree *tree = Tree::UnwrapTree(js_tree.As<Napi::Object>());
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  return MarshalNode(env, tree, node);
}

Napi::Value TreeCursor::Reset(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  Napi::Value js_tree = info.This().As<Napi::Object>()["tree"];
  const Tree *tree = Tree::UnwrapTree(js_tree.As<Napi::Object>());
  TSNode node = UnmarshalNode(env, tree);
  ts_tree_cursor_reset(&cursor_, node);
  return env.Undefined();
}

Napi::Value TreeCursor::NodeType(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  return Napi::String::New(env, ts_node_type(node));
}

Napi::Value TreeCursor::NodeIsNamed(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  return Napi::Boolean::New(env, ts_node_is_named(node));
}

Napi::Value TreeCursor::CurrentFieldName(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  const char *field_name = ts_tree_cursor_current_field_name(&cursor_);
  if (field_name) {
    return Napi::String::New(env, field_name);
  } else {
    return env.Undefined();
  }
}

Napi::Value TreeCursor::StartIndex(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  return ByteCountToJS(env, ts_node_start_byte(node));
}

Napi::Value TreeCursor::EndIndex(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSNode node = ts_tree_cursor_current_node(&cursor_);
  return ByteCountToJS(env, ts_node_end_byte(node));
}

Napi::Value TreeCursor::NewTreeCursor(TSTreeCursor cursor) {
  Napi::Object js_cursor = constructor->Value().New({});
  TreeCursor::Unwrap(js_cursor)->cursor_ = cursor;
  return js_cursor;
}

}  // namespace node_tree_sitter
