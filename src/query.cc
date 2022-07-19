#include "./query.h"
#include <string>
#include <vector>
#include <napi.h>
#include "./node.h"
#include "./language.h"
#include "./logger.h"
#include "./util.h"
#include "./conversions.h"

namespace node_tree_sitter {

using std::vector;
using namespace Napi;

static const int PREDICATE_FIRST = 0;
static const int PREDICATE_SECOND = 2;
static const int PREDICATE_THIRD = 4;

TSQueryCursor * Query::ts_query_cursor;
Napi::FunctionReference * Query::constructor = new Napi::FunctionReference();

const char *query_error_names[] = {
  "TSQueryErrorNone",
  "TSQueryErrorSyntax",
  "TSQueryErrorNodeType",
  "TSQueryErrorField",
  "TSQueryErrorCapture",
  "TSQueryErrorStructure",
};

TSQuery *UnwrapTSQuery(const Napi::Value &value) {
  return static_cast<TSQuery *>(
    GetInternalFieldPointer(value)
  );
}

void Query::Init(Napi::Object &exports) {
  ts_query_cursor = ts_query_cursor_new();
  Napi::Env env = exports.Env();
  Napi::Function ctor = DefineClass(env, "Query", {
    InstanceMethod("_matches", &Query::Matches, napi_writable),
    InstanceMethod("_captures", &Query::Captures, napi_writable),
    InstanceMethod<&Query::PredicatesAccessor>("predicates"),
    InstanceMethod<&Query::SetPropertiesAccessor>("setProperties"),
    InstanceMethod<&Query::AssertedPropertiesAccessor>("assertedProperties"),
    InstanceMethod<&Query::RefutedPropertiesAccessor>("refutedProperties"),
  });

  *constructor = Napi::Persistent(ctor);
  exports["Query"] = ctor;
}

Query::Query(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Query>(info) {
  const TSLanguage *language = language_methods::UnwrapLanguage(info[0]);
  const char *source;
  uint32_t source_len;
  uint32_t error_offset = 0;
  TSQueryError error_type = TSQueryErrorNone;
  TSQuery *query;

  if (language == nullptr) {
    Napi::Error::New(info.Env(), "Missing language argument").ThrowAsJavaScriptException();
    return;
  }

  if (info[1].IsString()) {
    std::string utf8_string = info[1].ToString().Utf8Value();
    source = utf8_string.c_str();
    source_len = utf8_string.length();
    query = ts_query_new(language, source, source_len, &error_offset, &error_type);
  }
  else if (info[1].IsBuffer()) {
    source = info[1].As<Napi::Buffer<char>>().Data();
    source_len = info[1].As<Napi::Buffer<char>>().Length();
    query = ts_query_new(language, source, source_len, &error_offset, &error_type);
  }
  else {
    Napi::Error::New(info.Env(), "Missing source argument").ThrowAsJavaScriptException();
    return;
  }

  if (error_offset > 0) {
    const char *error_name = query_error_names[error_type];
    std::string message = "Query error of type ";
    message += error_name;
    message += " at position ";
    message += std::to_string(error_offset);
    Napi::Error::New(info.Env(), message).ThrowAsJavaScriptException();
    return;
  }

  // info.This().ToObject().Get("_init").As<Napi::Function>().Call(info.This(), {});

  this->query_ = query;

  BuildPredicates(GetPredicates(info.Env()));
}

Query::~Query() {
  ts_query_delete(query_);
}

Query* Query::UnwrapQuery(Napi::Value const &value) {
  return Query::Unwrap(value.ToObject());
}

Napi::Value Query::PredicatesAccessor(const Napi::CallbackInfo &info) {
  return jsPredicates->Value();
}

Napi::Value Query::SetPropertiesAccessor(const Napi::CallbackInfo &info) {
  return jsSetProperties->Value();
}

Napi::Value Query::AssertedPropertiesAccessor(const Napi::CallbackInfo &info) {
  return jsAssertedProperties->Value();
}

Napi::Value Query::RefutedPropertiesAccessor(const Napi::CallbackInfo &info) {
  return jsRefutedProperties->Value();
}

void Query::BuildPredicates(Napi::Array descriptions) {
  const size_t count = descriptions.Length();
  Napi::Env env = descriptions.Env();

  Napi::Array setProperties = Napi::Array::New(env, count);
  Napi::Array assertedProperties = Napi::Array::New(env, count);
  Napi::Array refutedProperties = Napi::Array::New(env, count);
  Napi::Array predicates = Napi::Array::New(env, count);

  for (size_t i = 0; i < count; i++) {
    Napi::Array description = descriptions.Get(i).As<Napi::Array>();
    const size_t jCount = description.Length();

    Napi::Array predicate = Napi::Array::New(env);
    size_t predicateIndex = 0;

    Napi::Function regExp = env.Global().Get("RegExp").As<Napi::Function>();

    for (size_t j = 0; j < jCount; j++) {
      Napi::Array steps = description.Get(j).As<Napi::Array>();
      const size_t stepsLength = steps.Length() / 2;

      if (steps.Get(PREDICATE_FIRST).ToNumber().Uint32Value() != TSQueryPredicateStepTypeString) {
        Napi::TypeError::New(env, "Predicates must begin with a literal value").ThrowAsJavaScriptException();
        return;
      }

      std::string opCode = steps.Get(PREDICATE_FIRST + 1).ToString();

      const int argType1 = steps.Get(PREDICATE_SECOND).ToNumber().Int32Value();
      const int argType2 = steps.Get(PREDICATE_THIRD).ToNumber().Int32Value();
      Napi::Value argVal1 = steps.Get(PREDICATE_SECOND + 1);
      Napi::Value argVal2 = steps.Get(PREDICATE_THIRD + 1);

      if (opCode == "not-eq?" || opCode == "eq?") {
        if (stepsLength != 3) {
          Napi::Error::New(env, "Wrong number of arguments to `#eq?` predicate. Expected 2, got " + (stepsLength - 1))
            .ThrowAsJavaScriptException();
          return;
        }

        if (argType1 != TSQueryPredicateStepTypeCapture) {
          Napi::Error::New(env, "First argument of `#eq?` predicate must be a capture. Got " + argType1)
            .ThrowAsJavaScriptException();
        }
        
        Napi::Array predicateStep = Napi::Array::New(env);
        predicateStep.Set(0u, Napi::Boolean::New(env, true));
        predicateStep.Set(1u, argVal1);
        predicateStep.Set(2u, argVal2);
        predicateStep.Set(3u, Napi::Boolean::New(env, argType2 == TSQueryPredicateStepTypeCapture));
        predicateStep.Set(4u, Napi::Boolean::New(env, opCode == "not-eq?"));
        predicate.Set(predicateIndex++, predicateStep);
      } else if (opCode == "match?") {
        if (stepsLength != 3) {
          Napi::Error::New(env, "Wrong number of arguments to `#match?` predicate. Expected 2, got " + (stepsLength - 1))
            .ThrowAsJavaScriptException();
          return;
        }

        if (argType1 != TSQueryPredicateStepTypeCapture) {
          std::string tmp = argVal1.ToString().Utf8Value();
          Napi::Error::New(env, "First argument of `#match?` predicate must be a capture. Got  " + tmp)
            .ThrowAsJavaScriptException();
          return;
        }

        if (argType2 != TSQueryPredicateStepTypeString) {
          std::string tmp = argVal2.ToString().Utf8Value();
          Napi::Error::New(env, "Second argument of `#match?` predicate must be a string. Got  " + tmp)
            .ThrowAsJavaScriptException();
          return;
        }
        Napi::Array predicateStep = Napi::Array::New(env);
        predicateStep.Set(0u, Napi::Boolean::New(env, false));
        predicateStep.Set(1u, argVal1);
        predicateStep.Set(2u, regExp.Call({argVal2}));
        predicate.Set(predicateIndex++, predicateStep);
      } else if (opCode == "set!") {
        if (stepsLength != 2 && stepsLength != 3) {
          Napi::Error::New(env, "Wrong number of arguments to `#set!` predicate. Expected 1 or 2. Got " + (stepsLength - 1))
            .ThrowAsJavaScriptException();
          return;
        }

        for (size_t stepIndex = 0; stepIndex < stepsLength; stepIndex += 2) {
          if (steps.Get(stepIndex).ToNumber().Uint32Value() != TSQueryPredicateStepTypeString) {
            Napi::Error::New(env, "Arguments to `#set!` predicate must be a strings.").ThrowAsJavaScriptException();
            return;
          }
        }

        if (!setProperties.Has(i)) {
          setProperties.Set(i, Napi::Object::New(env));
        }
        setProperties.Get(i).ToObject().Set(argVal1, argType2 ? argVal2 : env.Null());
      } else if (opCode == "is?" || opCode == "is-not?") {
        if (stepsLength != 2 && stepsLength != 3) {
          Napi::Error::New(env, "Wrong number of arguments to `#" + opCode + "` predicate. Expected 1 or 2. Got " + std::to_string(stepsLength - 1))
            .ThrowAsJavaScriptException();
          return;
        }

        for (size_t stepIndex = 0; stepIndex < stepsLength; stepIndex += 2) {
          if (steps.Get(stepIndex).ToNumber().Uint32Value() != TSQueryPredicateStepTypeString) {
            Napi::Error::New(env, "Arguments to `#" + opCode + "` predicate must be a strings.").ThrowAsJavaScriptException();
            return;
          }
        }
        Napi::Array properties = opCode == "is?" ? assertedProperties : refutedProperties;
        if (!properties.Has(i)) {
          properties.Set(i, Napi::Object::New(env));
        }
        properties.Get(i).ToObject().Set(argVal1, argType2 ? argVal2 : env.Null());
      } else {
        std::string tmp = steps.Get(PREDICATE_FIRST + 1).ToString().Utf8Value();
        Napi::Error::New(env, "Unknown query predicate `#" + tmp + "`").ThrowAsJavaScriptException();
        return;
      }
    }
    predicates.Set(i, predicate);
  }

  *jsPredicates = Napi::Persistent(predicates);
  *jsSetProperties = Napi::Persistent(setProperties);
  *jsRefutedProperties = Napi::Persistent(refutedProperties);
  *jsAssertedProperties = Napi::Persistent(assertedProperties);
}

Napi::Array Query::GetPredicates(Napi::Env env) {
  auto ts_query = this->query_;

  auto pattern_len = ts_query_pattern_count(ts_query);

  Napi::Array js_predicates = Napi::Array::New(env);

  for (size_t pattern_index = 0; pattern_index < pattern_len; pattern_index++) {
    uint32_t predicates_len;
    const TSQueryPredicateStep *predicates = ts_query_predicates_for_pattern(
        ts_query, pattern_index, &predicates_len);

    Napi::Array js_pattern_predicates = Napi::Array::New(env);

    if (predicates_len > 0) {
      Napi::Array js_predicate = Napi::Array::New(env);

      size_t a_index = 0;
      size_t p_index = 0;
      for (size_t i = 0; i < predicates_len; i++) {
        const TSQueryPredicateStep predicate = predicates[i];
        uint32_t len;
        switch (predicate.type) {
          case TSQueryPredicateStepTypeCapture:
            js_predicate.Set(p_index++, Napi::Number::New(env, (int) TSQueryPredicateStepTypeCapture));
            js_predicate.Set(p_index++,
                Napi::String::New(env,
                  ts_query_capture_name_for_id(ts_query, predicate.value_id, &len)
                ));
            break;
          case TSQueryPredicateStepTypeString:
            js_predicate.Set(p_index++, Napi::Number::New(env, (int) TSQueryPredicateStepTypeString));
            js_predicate.Set(p_index++,
                Napi::String::New(env, 
                  ts_query_string_value_for_id(ts_query, predicate.value_id, &len)
                ));
            break;
          case TSQueryPredicateStepTypeDone:
            js_pattern_predicates.Set(a_index++, js_predicate);
            js_predicate = Napi::Array::New(env);
            p_index = 0;
            break;
        }
      }
    }

    js_predicates.Set(pattern_index, js_pattern_predicates);
  }

  return js_predicates;
}

Napi::Value Query::Matches(Napi::CallbackInfo const &info) {
  const Tree *tree = Tree::UnwrapTree(info[0]);
  uint32_t start_row    = info[1].ToNumber().Uint32Value();
  uint32_t start_column = info[2].ToNumber().Uint32Value() << 1;
  uint32_t end_row      = info[3].ToNumber().Uint32Value();
  uint32_t end_column   = info[4].ToNumber().Uint32Value() << 1;

  // if (query == nullptr) {
  //   Napi::Error::New(info.Env(), "Missing argument query").ThrowAsJavaScriptException();
  //   return info.Env().Null();
  // }

  if (tree == nullptr) {
    Napi::Error::New(info.Env(), "Missing argument tree").ThrowAsJavaScriptException();
    return info.Env().Null();
  }

  TSQuery *ts_query = this->query_;
  TSNode rootNode = UnmarshalNode(info.Env(), tree);
  TSPoint start_point = {start_row, start_column};
  TSPoint end_point = {end_row, end_column};
  ts_query_cursor_set_point_range(ts_query_cursor, start_point, end_point);
  ts_query_cursor_exec(ts_query_cursor, ts_query, rootNode);

  Napi::Array js_matches = Napi::Array::New(info.Env());
  unsigned index = 0;
  vector<TSNode> nodes;
  TSQueryMatch match;

  while (ts_query_cursor_next_match(ts_query_cursor, &match)) {
    js_matches.Set(index++, match.pattern_index);

    for (uint16_t i = 0; i < match.capture_count; i++) {
      const TSQueryCapture &capture = match.captures[i];

      uint32_t capture_name_len = 0;
      const char *capture_name = ts_query_capture_name_for_id(
          ts_query, capture.index, &capture_name_len);

      TSNode node = capture.node;
      nodes.push_back(node);

      Napi::Value js_capture = Napi::String::New(info.Env(), capture_name);
      js_matches.Set(index++, js_capture);
    }
  }

  auto js_nodes = MarshalNodes(info.Env(), tree, nodes.data(), nodes.size());

  auto result = Napi::Array::New(info.Env());
  int i = 0;
  result.Set(i++, js_matches);
  result.Set(i++, js_nodes);
  return result;
}

Napi::Value Query::Captures(const Napi::CallbackInfo &info) {
  Query *query = Query::Unwrap(info.This().ToObject());
  const Tree *tree = Tree::UnwrapTree(info[0]);
  uint32_t start_row    = info[1].ToNumber().Uint32Value();
  uint32_t start_column = info[2].ToNumber().Uint32Value() << 1;
  uint32_t end_row      = info[3].ToNumber().Uint32Value();
  uint32_t end_column   = info[4].ToNumber().Uint32Value() << 1;

  if (query == nullptr) {
    Napi::Error::New(info.Env(), "Missing argument query").ThrowAsJavaScriptException();
    return info.Env().Null();
  }

  if (tree == nullptr) {
    Napi::Error::New(info.Env(), "Missing argument tree").ThrowAsJavaScriptException();
    return info.Env().Null();
  }

  TSQuery *ts_query = query->query_;
  TSNode rootNode = UnmarshalNode(info.Env(), tree);
  TSPoint start_point = {start_row, start_column};
  TSPoint end_point = {end_row, end_column};
  ts_query_cursor_set_point_range(ts_query_cursor, start_point, end_point);
  ts_query_cursor_exec(ts_query_cursor, ts_query, rootNode);

  Napi::Array js_matches = Napi::Array::New(info.Env());
  unsigned index = 0;
  vector<TSNode> nodes;
  TSQueryMatch match;
  uint32_t capture_index;

  while (ts_query_cursor_next_capture(
    ts_query_cursor,
    &match,
    &capture_index
  )) {

    js_matches.Set(index++, match.pattern_index);
    js_matches.Set(index++, capture_index);

    for (uint16_t i = 0; i < match.capture_count; i++) {
      const TSQueryCapture &capture = match.captures[i];

      uint32_t capture_name_len = 0;
      const char *capture_name = ts_query_capture_name_for_id(
          ts_query, capture.index, &capture_name_len);

      TSNode node = capture.node;
      nodes.push_back(node);

      js_matches.Set(index++, Napi::String::New(info.Env(), capture_name));
    }
  }

  auto js_nodes = MarshalNodes(info.Env(), tree, nodes.data(), nodes.size());

  auto result = Napi::Array::New(info.Env());
  int i = 0;
  (result).Set(i++, js_matches);
  (result).Set(i++, js_nodes);
  return result;
}

}  // namespace node_tree_sitter
