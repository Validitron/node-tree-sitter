#include "./language.h"
#include <napi.h>
#include <tree_sitter/api.h>
#include <vector>
#include <string>
#include "./util.h"

namespace node_tree_sitter {
namespace language_methods {

using std::vector;
using namespace Napi;

const TSLanguage *UnwrapLanguage(const Napi::Value &value) {
  Napi::Env env = value.Env();
  if (!value.IsObject()) {
    Napi::TypeError::New(env, "Invalid language").ThrowAsJavaScriptException();
    return nullptr;
  }

  Napi::Object obj = value.ToObject();
  if (!obj.Has("instance")) {
    Napi::TypeError::New(env, "Invalid language").ThrowAsJavaScriptException();
    return nullptr;
  }

  Napi::External<TSLanguage> instance = obj.Get("instance").As<Napi::External<TSLanguage>>();
  const TSLanguage *language = instance.Data();

  if (language) {
    uint32_t version = ts_language_version(language);
    if (
      version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION ||
      version > TREE_SITTER_LANGUAGE_VERSION
    ) {
      std::string message =
        "Incompatible language version. Compatible range: " +
        std::to_string(TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) + " - " +
        std::to_string(TREE_SITTER_LANGUAGE_VERSION) + ". Got: " +
        std::to_string(ts_language_version(language));
      Napi::RangeError::New(env, message.c_str()).ThrowAsJavaScriptException();
      return nullptr;
    }
    return language;
  }

  return nullptr;
}

static Napi::Value GetNodeTypeNamesById(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  const TSLanguage *language = UnwrapLanguage(info[0]);
  if (!language) {
    return env.Null();
  }

  Napi::Array result = Napi::Array::New(env);
  uint32_t length = ts_language_symbol_count(language);
  for (uint32_t i = 0; i < length; i++) {
    const char *name = ts_language_symbol_name(language, i);
    TSSymbolType type = ts_language_symbol_type(language, i);
    if (type == TSSymbolTypeRegular) {
      result[i] = Napi::String::New(env, name);
    } else {
      result[i] = env.Null();
    }
  }

  return result;
}

static Napi::Value GetNodeFieldNamesById(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  const TSLanguage *language = UnwrapLanguage(info[0]);
  if (!language) return env.Null();

  Napi::Array result = Napi::Array::New(env);
  uint32_t length = ts_language_field_count(language);
  for (uint32_t i = 0; i < length + 1; i++) {
    const char *name = ts_language_field_name_for_id(language, i);
    if (name) {
      result[i] = Napi::String::New(env, name);
    } else {
      result[i] = env.Null();
    }
  }

  return result;
}

void InitLanguage(Napi::Object &exports) {
  Napi::Env env = exports.Env();
  exports["getNodeTypeNamesById"] = Napi::Function::New(env, GetNodeTypeNamesById);
  exports["getNodeFieldNamesById"] = Napi::Function::New(env, GetNodeFieldNamesById);
}

}  // namespace language_methods
}  // namespace node_tree_sitter
