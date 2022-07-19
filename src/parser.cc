#include "./parser.h"
#include <string>
#include <vector>
#include <climits>
#include <napi.h>
#include <v8.h>
#include "./conversions.h"
#include "./language.h"
#include "./logger.h"
#include "./node.h"
#include "./tree.h"
#include "./util.h"
#include <cmath>

namespace node_tree_sitter {

using namespace Napi;
using std::vector;
using std::pair;

Napi::FunctionReference * Parser::constructor = new Napi::FunctionReference();

void Parser::Init(Napi::Object &exports) {
  Napi::Env env = exports.Env();

  Napi::Function ctor = DefineClass(env, "Parser", {
    InstanceMethod("getLogger", &Parser::GetLogger, napi_writable),
    InstanceMethod("setLogger", &Parser::SetLogger, napi_writable),
    InstanceMethod("setLanguage", &Parser::SetLanguage, napi_writable),
    InstanceMethod("printDotGraphs", &Parser::PrintDotGraphs, napi_writable),
    InstanceMethod("parse", &Parser::Parse, napi_writable),
  });

  Napi::String s = Napi::String::New(env, "");
  if (env.IsExceptionPending()) {
    return;
  }

  napi_value value;
  napi_valuetype type;
  napi_status status = napi_get_property(
    env,
    s,
    Napi::String::New(env, "slice"),
    &value
  );

  if (status != napi_ok) {
    Napi::Error::New(env, "node_tree_sitter::Parser failed to initialize.").ThrowAsJavaScriptException();
    return;
  }

  status = napi_typeof(env, value, &type);

  if (status != napi_ok) {
    Napi::Error::New(env, "node_tree_sitter::Parser failed to initialize.").ThrowAsJavaScriptException();
    return;
  }

  *constructor = Napi::Persistent(ctor);
  exports["Parser"] = ctor;
  exports["LANGUAGE_VERSION"] = Napi::Number::New(env, TREE_SITTER_LANGUAGE_VERSION);
}

Parser::Parser(const Napi::CallbackInfo &info)
  : Napi::ObjectWrap<Parser>(info),
    parser_(ts_parser_new()),
    is_parsing_async_(false)
    {}

Parser::~Parser() { ts_parser_delete(parser_); }

class CallbackInput {
  public:
  CallbackInput(Napi::Function callback, Napi::Value js_buffer_size)
    : byte_offset(0), 
    callback(Napi::Persistent(callback)) {
    if (js_buffer_size.IsNumber()) {
      buffer.resize(js_buffer_size.As<Napi::Number>().Uint32Value());
    } else {
      buffer.resize(32 * 1024);
    }
  }

  TSInput Input() {
    TSInput result;
    result.payload = (void *)this;
    result.encoding = TSInputEncodingUTF16;
    result.read = Read;
    return result;
  }

  private:
  static const char * Read(void *payload, uint32_t byte, TSPoint position, uint32_t *bytes_read) {
    int i = 0;
    CallbackInput *reader = (CallbackInput *)payload;
    Napi::Env env = reader->callback.Env();

    if (byte != reader->byte_offset) {
      reader->byte_offset = byte;
      reader->partial_string.Reset();
    }

    *bytes_read = 0;
    Napi::String result;
    if (!reader->partial_string.IsEmpty()) {
      result = reader->partial_string.Value().As<Napi::String>();
    } else {
      Napi::Number byteCount = ByteCountToJS(env, byte);
      Napi::Value result_value = reader->callback.Call({
        byteCount,
        PointToJS(env, position),
      });
      if (env.IsExceptionPending()) {
        return nullptr;
      }
      if (!result_value.IsString()) {
        return nullptr;
      }
      result = result_value.As<Napi::String>();
    }

    size_t length = 0;
    size_t utf16_units_read = 0;
    napi_status status;
    status = napi_get_value_string_utf16(
      env, result, nullptr, 0, &length
    );
    if (status != napi_ok) return nullptr;
    status = napi_get_value_string_utf16(
      env, result, (char16_t *)&reader->buffer[0], reader->buffer.size(), &utf16_units_read
    );
    if (status != napi_ok) return nullptr;

    *bytes_read = 2 * utf16_units_read;
    reader->byte_offset += *bytes_read;

    if (utf16_units_read < length) {
      reader->partial_string = Napi::Persistent(result);
    } else {
      reader->partial_string.Reset();
    }

    return (const char *)reader->buffer.data();
  }

  FunctionReference callback;
  std::vector<uint16_t> buffer;
  size_t byte_offset;
  Reference<Napi::String> partial_string;
};

class TextBufferInput {
 public:
  TextBufferInput(const vector<pair<const char16_t *, uint32_t>> *slices)
    : slices_(slices),
      byte_offset(0),
      slice_index_(0),
      slice_offset_(0) {}

  TSInput input() {
    return TSInput{this, Read, TSInputEncodingUTF16};
  }

 private:
  void seek(uint32_t byte_offset) {
    this->byte_offset = byte_offset;

    uint32_t total_length = 0;
    uint32_t goal_index = byte_offset / 2;
    for (unsigned i = 0, n = this->slices_->size(); i < n; i++) {
      uint32_t next_total_length = total_length + this->slices_->at(i).second;
      if (next_total_length > goal_index) {
        this->slice_index_ = i;
        this->slice_offset_ = goal_index - total_length;
        return;
      }
      total_length = next_total_length;
    }

    this->slice_index_ = this->slices_->size();
    this->slice_offset_ = 0;
  }

  static const char *Read(void *payload, uint32_t byte, TSPoint position, uint32_t *length) {
    auto self = static_cast<TextBufferInput *>(payload);

    if (byte != self->byte_offset) self->seek(byte);

    if (self->slice_index_ == self->slices_->size()) {
      *length = 0;
      return "";
    }

    auto &slice = self->slices_->at(self->slice_index_);
    const char16_t *result = slice.first + self->slice_offset_;
    *length = 2 * (slice.second - self->slice_offset_);
    self->byte_offset += *length;
    self->slice_index_++;
    self->slice_offset_ = 0;
    return reinterpret_cast<const char *>(result);
  }

  const vector<pair<const char16_t *, uint32_t>> *slices_;
  uint32_t byte_offset;
  uint32_t slice_index_;
  uint32_t slice_offset_;
};

bool Parser::handle_included_ranges(Napi::Value arg) {
  Napi::Env env = arg.Env();
  uint32_t last_included_range_end = 0;
  if (arg.IsArray()) {
    Napi::Array js_included_ranges = arg.As<Napi::Array>();
    vector<TSRange> included_ranges;
    for (unsigned i = 0; i < js_included_ranges.Length(); i++) {
      Napi::Value range_value = js_included_ranges[i];
      if (!range_value.IsObject()) return false;
      optional<TSRange> range = RangeFromJS(range_value);
      if (!range) return false;
      if (range->start_byte < last_included_range_end) {
        Napi::Error::New(env, "Overlapping ranges").ThrowAsJavaScriptException();
        return false;
      }
      last_included_range_end = range->end_byte;
      included_ranges.push_back(*range);
    }
    ts_parser_set_included_ranges(parser_, included_ranges.data(), included_ranges.size());
  } else {
    ts_parser_set_included_ranges(parser_, nullptr, 0);
  }
  return true;
}

Napi::Value Parser::SetLanguage(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  if (is_parsing_async_) {
    Napi::Error::New(env, "Parser is in use").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const TSLanguage *language = language_methods::UnwrapLanguage(info[0]);
  if (language) ts_parser_set_language(parser_, language);
  return info.This();
}

Napi::Value Parser::Parse(const Napi::CallbackInfo &info) {
  auto env = info.Env();

  if (is_parsing_async_) {
    Napi::Error::New(env, "Parser is in use").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env, "Input must be a function").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  const TSTree *old_tree = nullptr;
  if (info.Length() > 1 && info[1].IsObject()) {
    const Tree *tree = Tree::UnwrapTree(info[1]);
    if (!tree) {
      Napi::TypeError::New(env, "Second argument must be a tree").ThrowAsJavaScriptException();
      return env.Undefined();
    }
    old_tree = tree->tree_;
  }

  auto buffer_size = env.Null();
  if (info.Length() > 2 && info[2].IsNumber()) buffer_size = info[2];
  if (!handle_included_ranges(info[3])) return env.Undefined();

  CallbackInput callback_input(callback, buffer_size);
  TSTree *tree = ts_parser_parse(parser_, old_tree, callback_input.Input());
  Napi::Value ref = Napi::External<TSTree>::New(env, tree);
  return Tree::constructor->New({ref});
}

class ParseWorker : public Napi::AsyncWorker {
  Parser *parser_;
  TSTree *new_tree_;
  TextBufferInput *input_;

public:
  ParseWorker(Napi::Function &callback, Parser *parser, TextBufferInput *input) :
    Napi::AsyncWorker(callback, "tree-sitter.parseTextBuffer"),
    parser_(parser),
    new_tree_(nullptr),
    input_(input) {}

  void Execute() {
    TSLogger logger = ts_parser_logger(parser_->parser_);
    ts_parser_set_logger(parser_->parser_, TSLogger{0, 0});
    new_tree_ = ts_parser_parse(parser_->parser_, nullptr, input_->input());
    ts_parser_set_logger(parser_->parser_, logger);
  }

  void OnOK() {
    parser_->is_parsing_async_ = false;
    delete input_;
    Napi::Value ref = Napi::External<TSTree>::New(Env(), new_tree_);
    Callback()({Tree::constructor->New({ref})});
  }
};

Napi::Value Parser::GetLogger(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  TSLogger current_logger = ts_parser_logger(parser_);
  if (current_logger.payload && current_logger.log == Logger::Log) {
    Logger *logger = (Logger *)current_logger.payload;
    return logger->func.Value();
  } else {
    return env.Null();
  }
}

Napi::Value Parser::SetLogger(const Napi::CallbackInfo &info) {
  auto env = info.Env();

  if (is_parsing_async_) {
    Napi::Error::New(env, "Parser is in use").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  TSLogger current_logger = ts_parser_logger(parser_);

  if (info[0].IsFunction()) {
    if (current_logger.payload) delete (Logger *)current_logger.payload;
    ts_parser_set_logger(parser_, Logger::Make(info[0].As<Napi::Function>()));
  } else if (info[0].IsEmpty() || info[0].IsNull() || (info[0].IsBoolean() && !info[0].As<Napi::Boolean>())) {
    if (current_logger.payload) delete (Logger *)current_logger.payload;
    ts_parser_set_logger(parser_, { 0, 0 });
  } else {
    Napi::TypeError::New(env, "Logger callback must either be a function or a falsy value").ThrowAsJavaScriptException();
  }

  return info.This();
}

Napi::Value Parser::PrintDotGraphs(const Napi::CallbackInfo &info) {
  auto env = info.Env();

  if (is_parsing_async_) {
    Napi::Error::New(env, "Parser is in use").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (info[0].IsBoolean() && info[0].As<Napi::Boolean>()) {
    ts_parser_print_dot_graphs(parser_, 2);
  } else {
    ts_parser_print_dot_graphs(parser_, -1);
  }

  return info.This();
}

// static Napi::FunctionReference string_slice;

}  // namespace node_tree_sitter
