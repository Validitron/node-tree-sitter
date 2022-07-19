#ifndef NODE_TREE_SITTER_LANGUAGE_H_
#define NODE_TREE_SITTER_LANGUAGE_H_

#include <napi.h>
#include <tree_sitter/api.h>
#include "./tree.h"

namespace node_tree_sitter {
namespace language_methods {

void InitLanguage(Napi::Object &);

const TSLanguage *UnwrapLanguage(const Napi::Value &);

}  // namespace language_methods
}  // namespace node_tree_sitter

#endif  // NODE_TREE_SITTER_LANGUAGE_H_
