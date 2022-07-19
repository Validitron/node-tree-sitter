#ifndef NODE_TREE_SITTER_PARSER_H_
#define NODE_TREE_SITTER_PARSER_H_

#include <napi.h>
#include <tree_sitter/api.h>

namespace node_tree_sitter {

class Parser : public Napi::ObjectWrap<Parser> {
 public:
  static void Init(Napi::Object &);
  Parser(const Napi::CallbackInfo&);
  ~Parser();
  
  Napi::Value GetLogger(const Napi::CallbackInfo &);
  Napi::Value SetLogger(const Napi::CallbackInfo &);
  Napi::Value SetLanguage(const Napi::CallbackInfo &);
  Napi::Value PrintDotGraphs(const Napi::CallbackInfo &);
  Napi::Value Parse(const Napi::CallbackInfo &);

  static Napi::FunctionReference * constructor;

  TSParser *parser_;
  bool is_parsing_async_;

 private:

  bool handle_included_ranges(class Napi::Value arg);
};

}  // namespace node_tree_sitter

#endif  // NODE_TREE_SITTER_PARSER_H_
