#ifndef NODE_TREE_SITTER_QUERY_H_
#define NODE_TREE_SITTER_QUERY_H_

#include <napi.h>
#include <node_object_wrap.h>
#include <unordered_map>
#include <tree_sitter/api.h>

namespace node_tree_sitter {

class Query : public Napi::ObjectWrap<Query> {
 public:
  static void Init(Napi::Object &);
  Query(const Napi::CallbackInfo &);
  ~Query();

  // Napi::Value GetPredicates(const Napi::CallbackInfo &);
  Napi::Value Matches(const Napi::CallbackInfo &);
  Napi::Value Captures(const Napi::CallbackInfo &);
  Napi::Value Parse(const Napi::CallbackInfo &);
  Napi::Value PredicatesAccessor(const Napi::CallbackInfo &);
  Napi::Value SetPropertiesAccessor(const Napi::CallbackInfo &);
  Napi::Value AssertedPropertiesAccessor(const Napi::CallbackInfo &);
  Napi::Value RefutedPropertiesAccessor(const Napi::CallbackInfo &);

  static TSQueryCursor *ts_query_cursor;
  static Napi::FunctionReference * constructor;

  static Query* UnwrapQuery(Napi::Value const &);
  
  TSQuery *query_;

 private:
  Napi::Array GetPredicates(Napi::Env env);
  void BuildPredicates(Napi::Array);
  Napi::Reference<Napi::Array>* jsPredicates = new Napi::Reference<Napi::Array>();
  Napi::Reference<Napi::Array>* jsSetProperties = new Napi::Reference<Napi::Array>();
  Napi::Reference<Napi::Array>* jsAssertedProperties = new Napi::Reference<Napi::Array>();
  Napi::Reference<Napi::Array>* jsRefutedProperties = new Napi::Reference<Napi::Array>();
};

}  // namespace node_tree_sitter

#endif  // NODE_TREE_SITTER_QUERY_H_
