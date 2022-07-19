#include "./logger.h"
#include <string>
#include <napi.h>
#include <tree_sitter/api.h>

namespace node_tree_sitter {

using namespace Napi;
using std::string;

void Logger::Log(void *payload, TSLogType type, const char *message_str) {
  Logger *debugger = (Logger *)payload;
  Napi::Function fn = debugger->func.Value();
  if (!fn.IsFunction()) return;
  Napi::Env env = fn.Env();

  string message(message_str);
  string param_sep = " ";
  size_t param_sep_pos = message.find(param_sep, 0);

  Napi::String type_name = Napi::String::New(
    env,
    type == TSLogTypeParse ? "parse" : "lex"
  );
  Napi::String name = Napi::String::New(env, message.substr(0, param_sep_pos));
  Napi::Object params = Napi::Object::New(env);

  while (param_sep_pos != string::npos) {
    size_t key_pos = param_sep_pos + param_sep.size();
    size_t value_sep_pos = message.find(":", key_pos);
    if (value_sep_pos == string::npos) break;

    size_t val_pos = value_sep_pos + 1;
    param_sep = ", ";
    param_sep_pos = message.find(param_sep, value_sep_pos);

    string key = message.substr(key_pos, (value_sep_pos - key_pos));
    string value = message.substr(val_pos, (param_sep_pos - val_pos));
    params[key] = Napi::String::New(env, value);
  }

  fn({ name, params, type_name });
  if (env.IsExceptionPending()) {
    Napi::Error error = env.GetAndClearPendingException();
    Napi::Value console = env.Global()["console"];
    if (console.IsObject()) {
      Napi::Value console_error_fn = console.ToObject()["error"];
      if (console_error_fn.IsFunction()) {
        console_error_fn.As<Napi::Function>()({
          Napi::String::New(env, "Napi::Error in debug callback:"),
          error.Value()
        });
      }
    }
  }
}

TSLogger Logger::Make(Napi::Function func) {
  TSLogger result;
  Logger *logger = new Logger();
  logger->func.Reset(func, 1);
  result.payload = static_cast<void *>(logger);
  result.log = Log;
  return result;
}

}  // namespace node_tree_sitter
