#include "./node.h"
#include <tree_sitter/api.h>
#include <napi.h>
#include <vector>
#include "./util.h"
#include "./conversions.h"
#include "./tree.h"
#include "./tree_cursor.h"

namespace node_tree_sitter {

using std::vector;
using namespace Napi;

static const uint32_t FIELD_COUNT_PER_NODE = 6;

static uint32_t *transfer_buffer = nullptr;
static uint32_t transfer_buffer_length = 0;
static Napi::Reference<Napi::Uint32Array>* nodeTransferBuffer = new Napi::Reference<Napi::Uint32Array>();
static TSTreeCursor scratch_cursor = {nullptr, nullptr, {0, 0}};

Napi::Value NodeMethods::SetNodeTransferArray(const Napi::CallbackInfo &info ) {
  auto obj = nodeTransferBuffer->Value();
  obj.Set(info[0].ToNumber().Uint32Value(), info[1].ToNumber().Uint32Value());
  return obj;
}

Napi::Value NodeMethods::NodeTransferArray(const Napi::CallbackInfo &info) {
  return nodeTransferBuffer->Value();
}

static inline void setup_transfer_buffer(Napi::Env env, uint32_t node_count) {
  uint32_t new_length = node_count * FIELD_COUNT_PER_NODE;
  if (new_length > transfer_buffer_length) {
    if (transfer_buffer) {
      free(transfer_buffer);
    }
    transfer_buffer_length = new_length;
    transfer_buffer = static_cast<uint32_t *>(malloc(transfer_buffer_length * sizeof(uint32_t)));
    auto js_transfer_buffer = Napi::ArrayBuffer::New(
      env,
      transfer_buffer,
      transfer_buffer_length * sizeof(uint32_t)
    );
    *nodeTransferBuffer = Napi::Persistent(Napi::Uint32Array::New(
      env,
      transfer_buffer_length,
      js_transfer_buffer,
      0
    ));
  }
}

static inline bool operator<=(const TSPoint &left, const TSPoint &right) {
  if (left.row < right.row) return true;
  if (left.row > right.row) return false;
  return left.column <= right.column;
}

Napi::Value MarshalNodes(
  Napi::Env env,
  const Tree *tree,
  const TSNode *nodes,
  uint32_t node_count
) {
  Napi::Array result = Napi::Array::New(env, node_count);
  setup_transfer_buffer(env, node_count);
  uint32_t *p = transfer_buffer;
  for (unsigned i = 0; i < node_count; i++) {
    TSNode node = nodes[i];
    const auto &cache_entry = tree->cached_nodes_.find(node.id);
    if (cache_entry == tree->cached_nodes_.end()) {
      MarshalPointer(node.id, p);
      p += 2;
      *(p++) = node.context[0];
      *(p++) = node.context[1];
      *(p++) = node.context[2];
      *(p++) = node.context[3];
      if (node.id) {
        result[i] = Napi::Number::New(env, ts_node_symbol(node));
      } else {
        result[i] = env.Null();
      }
    } else {
      result[i] = cache_entry->second->node.Value();
    }
  }

  return result;
}

Napi::Value MarshalNode(
  Napi::Env env,
  const Tree *tree,
  TSNode node
) {
  const auto &cache_entry = tree->cached_nodes_.find(node.id);
  if (cache_entry == tree->cached_nodes_.end()) {
    setup_transfer_buffer(env, 1);
    uint32_t *p = transfer_buffer;
    MarshalPointer(node.id, p);
    p += 2;
    *(p++) = node.context[0];
    *(p++) = node.context[1];
    *(p++) = node.context[2];
    *(p++) = node.context[3];
    if (node.id) {
      return Napi::Number::New(env, ts_node_symbol(node));
    } else {
      return env.Null();
    }
  } else {
    return cache_entry->second->node.Value();
  }
  return env.Null();
}

Napi::Value MarshalNullNode(Napi::Env env) {
  memset(transfer_buffer, 0, FIELD_COUNT_PER_NODE * sizeof(transfer_buffer[0]));
  return env.Null();
}

TSNode UnmarshalNode(Napi::Env env, const Tree *tree) {
  TSNode result = {{0, 0, 0, 0}, nullptr, nullptr};
  if (!tree) {
    Napi::TypeError::New(env, "Argument must be a tree").ThrowAsJavaScriptException();
    return result;
  }

  result.tree = tree->tree_;
  result.id = UnmarshalPointer(&transfer_buffer[0]);
  result.context[0] = transfer_buffer[2];
  result.context[1] = transfer_buffer[3];
  result.context[2] = transfer_buffer[4];
  result.context[3] = transfer_buffer[5];
  return result;
}

Napi::Value NodeMethods::ToString(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    char *string = ts_node_string(node);
    Napi::String result = Napi::String::New(env, string);
    free(string);
    return result;
  }
  return env.Undefined();
}

Napi::Value NodeMethods::IsMissing(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    bool result = ts_node_is_missing(node);
    return Napi::Boolean::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::HasChanges(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    bool result = ts_node_has_changes(node);
    return Napi::Boolean::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::HasError(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    bool result = ts_node_has_error(node);
    return Napi::Boolean::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::FirstNamedChildForIndex(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    auto byte = ByteCountFromJS(info[1]);
    if (byte) {
      return MarshalNode(env, tree, ts_node_first_named_child_for_byte(node, *byte));
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::FirstChildForIndex(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id && info.Length() > 1) {
    optional<uint32_t> byte = ByteCountFromJS(info[1]);
    if (byte) {
      return MarshalNode(env, tree, ts_node_first_child_for_byte(node, *byte));
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::NamedDescendantForIndex(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    optional<uint32_t> maybe_min = ByteCountFromJS(info[1]);
    if (maybe_min) {
      optional<uint32_t> maybe_max = ByteCountFromJS(info[2]);
      if (maybe_max) {
        uint32_t min = *maybe_min;
        uint32_t max = *maybe_max;
        return MarshalNode(env, tree, ts_node_named_descendant_for_byte_range(node, min, max));
      }
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::DescendantForIndex(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    optional<uint32_t> maybe_min = ByteCountFromJS(info[1]);
    if (maybe_min) {
      optional<uint32_t> maybe_max = ByteCountFromJS(info[2]);
      if (maybe_max) {
        uint32_t min = *maybe_min;
        uint32_t max = *maybe_max;
        return MarshalNode(env, tree, ts_node_descendant_for_byte_range(node, min, max));
      }
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::NamedDescendantForPosition(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    optional<TSPoint> maybe_min = PointFromJS(info[1]);
    optional<TSPoint> maybe_max = PointFromJS(info[2]);
    if (maybe_min && maybe_max) {
      TSPoint min = *maybe_min;
      TSPoint max = *maybe_max;
      return MarshalNode(env, tree, ts_node_named_descendant_for_point_range(node, min, max));
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::DescendantForPosition(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    optional<TSPoint> maybe_min = PointFromJS(info[1]);
    if (maybe_min) {
      optional<TSPoint> maybe_max = PointFromJS(info[2]);
      if (maybe_max) {
        TSPoint min = *maybe_min;
        TSPoint max = *maybe_max;
        return MarshalNode(env, tree, ts_node_descendant_for_point_range(node, min, max));
      }
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::Type(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    const char *result = ts_node_type(node);
    return Napi::String::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::TypeId(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    TSSymbol result = ts_node_symbol(node);
    return Napi::Number::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::IsNamed(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    bool result = ts_node_is_named(node);
    return Napi::Boolean::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::StartIndex(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    uint32_t result = ts_node_start_byte(node) / 2;
    return Napi::Number::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::EndIndex(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    uint32_t result = ts_node_end_byte(node) / 2;
    return Napi::Number::New(env, result);
  }
  return env.Undefined();
}

Napi::Value NodeMethods::StartPosition(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    TransferPoint(ts_node_start_point(node));
  }
  return env.Undefined();
}

Napi::Value NodeMethods::EndPosition(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    TransferPoint(ts_node_end_point(node));
  }
  return env.Undefined();
}

Napi::Value NodeMethods::Child(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    if (info[1].IsNumber()) {
      uint32_t index = info[1].As<Napi::Number>().Uint32Value();
      return MarshalNode(env, tree, ts_node_child(node, index));
    }
    Napi::TypeError::New(env, "Second argument must be an integer").ThrowAsJavaScriptException();
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::NamedChild(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    if (info[1].IsNumber()) {
      uint32_t index = info[1].As<Napi::Number>().Uint32Value();
      return MarshalNode(env, tree, ts_node_named_child(node, index));
    }
    Napi::TypeError::New(env, "Second argument must be an integer").ThrowAsJavaScriptException();
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::ChildCount(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return Napi::Number::New(env, ts_node_child_count(node));
  }
  return env.Undefined();
}

Napi::Value NodeMethods::NamedChildCount(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return Napi::Number::New(env, ts_node_named_child_count(node));
  }
  return env.Undefined();
}

Napi::Value NodeMethods::FirstChild(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_child(node, 0));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::FirstNamedChild(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_named_child(node, 0));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::LastChild(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    uint32_t child_count = ts_node_child_count(node);
    if (child_count > 0) {
      return MarshalNode(env, tree, ts_node_child(node, child_count - 1));
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::LastNamedChild(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    uint32_t child_count = ts_node_named_child_count(node);
    if (child_count > 0) {
      return MarshalNode(env, tree, ts_node_named_child(node, child_count - 1));
    }
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::Parent(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_parent(node));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::NextSibling(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_next_sibling(node));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::NextNamedSibling(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_next_named_sibling(node));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::PreviousSibling(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_prev_sibling(node));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::PreviousNamedSibling(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (node.id) {
    return MarshalNode(env, tree, ts_node_prev_named_sibling(node));
  }
  return MarshalNullNode(env);
}

struct SymbolSet {
  std::basic_string<TSSymbol> symbols;
  void add(TSSymbol symbol) { symbols += symbol; }
  bool contains(TSSymbol symbol) { return symbols.find(symbol) != symbols.npos; }
};

bool symbol_set_from_js(SymbolSet *symbols, const Napi::Value &value, const TSLanguage *language) {
  Napi::Env env = value.Env();
  if (!value.IsArray()) {
    Napi::TypeError::New(env, "Argument must be a string or array of strings").ThrowAsJavaScriptException();
    return false;
  }
  Napi::Array js_types = value.As<Napi::Array>();
  unsigned symbol_count = ts_language_symbol_count(language);
  for (uint32_t i = 0, n = js_types.Length(); i < n; i++) {
    Napi::Value js_node_type_value = js_types[i];
    if (js_node_type_value.IsString()) {
      Napi::String js_node_type = js_node_type_value.As<Napi::String>();
      std::string node_type = js_node_type.Utf8Value();
      if (node_type == "ERROR") {
        symbols->add(static_cast<TSSymbol>(-1));
      } else {
        for (TSSymbol j = 0; j < symbol_count; j++) {
          if (node_type == ts_language_symbol_name(language, j)) {
            symbols->add(j);
          }
        }
      }
    } else {
      Napi::TypeError::New(env, "Argument must be a string or array of strings").ThrowAsJavaScriptException();
      return false;
    }
  }
  return true;
}

Napi::Value NodeMethods::Children(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (!node.id) return env.Undefined();

  vector<TSNode> result;
  ts_tree_cursor_reset(&scratch_cursor, node);
  if (ts_tree_cursor_goto_first_child(&scratch_cursor)) {
    do {
      TSNode child = ts_tree_cursor_current_node(&scratch_cursor);
      result.push_back(child);
    } while (ts_tree_cursor_goto_next_sibling(&scratch_cursor));
  }

  return MarshalNodes(env, tree, result.data(), result.size());
}

Napi::Value NodeMethods::NamedChildren(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (!node.id) return env.Undefined();

  vector<TSNode> result;
  ts_tree_cursor_reset(&scratch_cursor, node);
  if (ts_tree_cursor_goto_first_child(&scratch_cursor)) {
    do {
      TSNode child = ts_tree_cursor_current_node(&scratch_cursor);
      if (ts_node_is_named(child)) {
        result.push_back(child);
      }
    } while (ts_tree_cursor_goto_next_sibling(&scratch_cursor));
  }

  return MarshalNodes(env, tree, result.data(), result.size());
}

Napi::Value NodeMethods::DescendantsOfType(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (!node.id) return env.Undefined();

  SymbolSet symbols;
  if (!symbol_set_from_js(&symbols, info[1], ts_tree_language(node.tree))) {
    return env.Undefined();
  }

  TSPoint start_point = {0, 0};
  TSPoint end_point = {UINT32_MAX, UINT32_MAX};
  if (info.Length() > 2 && info[2].IsObject()) {
    auto maybe_start_point = PointFromJS(info[2]);
    if (!maybe_start_point) return env.Undefined();
    start_point = *maybe_start_point;
  }

  if (info.Length() > 3 && info[3].IsObject()) {
    auto maybe_end_point = PointFromJS(info[3]);
    if (!maybe_end_point) return env.Undefined();
    end_point = *maybe_end_point;
  }

  vector<TSNode> found;
  ts_tree_cursor_reset(&scratch_cursor, node);
  auto already_visited_children = false;
  while (true) {
    TSNode descendant = ts_tree_cursor_current_node(&scratch_cursor);

    if (!already_visited_children) {
      if (ts_node_end_point(descendant) <= start_point) {
        if (ts_tree_cursor_goto_next_sibling(&scratch_cursor)) {
          already_visited_children = false;
        } else {
          if (!ts_tree_cursor_goto_parent(&scratch_cursor)) break;
          already_visited_children = true;
        }
        continue;
      }

      if (end_point <= ts_node_start_point(descendant)) break;

      if (symbols.contains(ts_node_symbol(descendant))) {
        found.push_back(descendant);
      }

      if (ts_tree_cursor_goto_first_child(&scratch_cursor)) {
        already_visited_children = false;
      } else if (ts_tree_cursor_goto_next_sibling(&scratch_cursor)) {
        already_visited_children = false;
      } else {
        if (!ts_tree_cursor_goto_parent(&scratch_cursor)) break;
        already_visited_children = true;
      }
    } else {
      if (ts_tree_cursor_goto_next_sibling(&scratch_cursor)) {
        already_visited_children = false;
      } else {
        if (!ts_tree_cursor_goto_parent(&scratch_cursor)) break;
      }
    }
  }

  return MarshalNodes(env, tree, found.data(), found.size());
}

Napi::Value NodeMethods::ChildNodesForFieldId(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (!node.id) return env.Undefined();

  if (!info[1].IsNumber()) {
    Napi::TypeError::New(env, "Second argument must be an integer").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  uint32_t field_id = info[1].As<Napi::Number>().Uint32Value();

  vector<TSNode> result;
  ts_tree_cursor_reset(&scratch_cursor, node);
  if (ts_tree_cursor_goto_first_child(&scratch_cursor)) {
    do {
      TSNode child = ts_tree_cursor_current_node(&scratch_cursor);
      if (ts_tree_cursor_current_field_id(&scratch_cursor) == field_id) {
        result.push_back(child);
      }
    } while (ts_tree_cursor_goto_next_sibling(&scratch_cursor));
  }

  return MarshalNodes(env, tree, result.data(), result.size());
}

Napi::Value NodeMethods::ChildNodeForFieldId(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);

  if (node.id) {
    if (!info[1].IsNumber()) {
      Napi::TypeError::New(env, "Second argument must be an integer").ThrowAsJavaScriptException();
      return env.Undefined();
    }
    uint32_t field_id = info[1].As<Napi::Number>().Uint32Value();
    return MarshalNode(env, tree, ts_node_child_by_field_id(node, field_id));
  }
  return MarshalNullNode(env);
}

Napi::Value NodeMethods::Closest(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  if (!node.id) return env.Undefined();

  SymbolSet symbols;
  if (!symbol_set_from_js(&symbols, info[1], ts_tree_language(node.tree))) {
    return env.Undefined();
  }

  for (;;) {
    TSNode parent = ts_node_parent(node);
    if (!parent.id) break;
    if (symbols.contains(ts_node_symbol(parent))) {
      return MarshalNode(env, tree, parent);
    }
    node = parent;
  }

  return MarshalNullNode(env);
}

Napi::Value NodeMethods::Walk(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  const Tree *tree = Tree::UnwrapTree(info[0]);
  TSNode node = UnmarshalNode(env, tree);
  TSTreeCursor cursor = ts_tree_cursor_new(node);
  return TreeCursor::NewTreeCursor(cursor);
}

Napi::FunctionReference * NodeMethods::constructor = new Napi::FunctionReference();

NodeMethods::NodeMethods(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<NodeMethods>(info)
    {}

Napi::Object NodeMethods::Init(Napi::Env env, Napi::Object &exports) {
  Napi::Function ctor = DefineClass(env, "NodeMethods", {
    StaticMethod<&NodeMethods::SetNodeTransferArray>("setNodeTransferArray", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NodeTransferArray>("nodeTransferArray", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::StartIndex>("startIndex", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::EndIndex>("endIndex", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::Type>("type", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::TypeId>("typeId", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::IsNamed>("isNamed", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::Parent>("parent", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::Child>("child", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NamedChild>("namedChild", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::Children>("children", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NamedChildren>("namedChildren", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::ChildCount>("childCount", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NamedChildCount>("namedChildCount", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::FirstChild>("firstChild", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::LastChild>("lastChild", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::FirstNamedChild>("firstNamedChild", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::LastNamedChild>("lastNamedChild", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NextSibling>("nextSibling", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NextNamedSibling>("nextNamedSibling", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::PreviousSibling>("previousSibling", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::PreviousNamedSibling>("previousNamedSibling", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::StartPosition>("startPosition", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::EndPosition>("endPosition", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::IsMissing>("isMissing", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::ToString>("toString", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::FirstChildForIndex>("firstChildForIndex", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::FirstNamedChildForIndex>("firstNamedChildForIndex", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::DescendantForIndex>("descendantForIndex", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NamedDescendantForIndex>("namedDescendantForIndex", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::DescendantForPosition>("descendantForPosition", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::NamedDescendantForPosition>("namedDescendantForPosition", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::HasChanges>("hasChanges", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::HasError>("hasError", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::DescendantsOfType>("descendantsOfType", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::Walk>("walk", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::Closest>("closest", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::ChildNodeForFieldId>("childNodeForFieldId", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
    StaticMethod<&NodeMethods::ChildNodesForFieldId>("childNodesForFieldId", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
  });

  *constructor = Napi::Persistent(ctor);

  exports["NodeMethods"] = ctor;

  return exports;
}

void InitNode(Napi::Env env, Napi::Object &exports) {
  NodeMethods::Init(env, exports);
  setup_transfer_buffer(env, 1);
}

}  // namespace node_tree_sitter
