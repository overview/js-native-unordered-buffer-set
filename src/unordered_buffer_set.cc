#include <deque>
#include <unordered_set>
#include <string>
#include <vector>

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <v8.h>

using namespace v8;

class UnorderedBufferSet : public node::ObjectWrap {
public:
  static void Init(Handle<Object> exports);
  static Persistent<Function> constructor;

  inline bool contains(const char* s, size_t len);
  std::vector<std::string> findAllMatches(const char* s, size_t len, size_t maxNgramSize);

private:
  std::unordered_set<std::string> set;

  explicit UnorderedBufferSet(const char* s, size_t len);

  void insertStringsSeparatedByNewlines(const char* s, size_t len);

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Contains(const FunctionCallbackInfo<Value>& args);
  static void FindAllMatches(const FunctionCallbackInfo<Value>& args);
};

Persistent<Function> UnorderedBufferSet::constructor;

UnorderedBufferSet::UnorderedBufferSet(const char* s, size_t len)
{
  this->insertStringsSeparatedByNewlines(s, len);
}

bool
UnorderedBufferSet::contains(const char* s, size_t len)
{
  const std::string str(s, len);
  return this->set.count(str) != 0;
}

static size_t
count_char_in_str(char ch, const char* s, size_t len) {
  size_t ret = 0;

  while (len > 0) {
    if (*s == ch) ret++;
    len--;
    s++;
  }

  return ret;
}

void
UnorderedBufferSet::insertStringsSeparatedByNewlines(const char* s, size_t len) {
  const char* start = s;
  const char* end = s + len;

  this->set.reserve(count_char_in_str('\n', s, len) + 1);

  for (;s < end; s++) {
    if (*s == '\n') {
      this->set.emplace(start, s);
      start = s + 1;
    }
  }

  if (start < end) {
    this->set.emplace(start, end);
  }
}

std::vector<std::string>
UnorderedBufferSet::findAllMatches(const char* s, size_t len, size_t maxNgramSize) {
  std::string str(s, len);
  std::vector<std::string> ret;
  std::deque<size_t> tokenStarts;
  size_t pos = 0;

  tokenStarts.push_back(0);

  while (pos < len) {
    pos = str.find(' ', pos);
    if (pos == std::string::npos) pos = len;

    // Add s[tokenStarts[0],pos), s[tokenStarts[1],pos), ... for every token
    // in the set
    for (auto i = tokenStarts.begin(); i < tokenStarts.end(); i++) {
      size_t tokenStart = *i;
      std::string needle(&s[tokenStart], &s[pos]);
      if (this->set.count(needle) != 0) {
        ret.push_back(needle);
      }
    }

    if (tokenStarts.size() == maxNgramSize) tokenStarts.pop_front();

    pos++;
    tokenStarts.push_back(pos);
  }

  return ret;
}

void
UnorderedBufferSet::Init(Handle<Object> exports) {
  Isolate* isolate = Isolate::GetCurrent();

  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
  tpl->SetClassName(String::NewFromUtf8(isolate, "UnorderedBufferSet"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "contains", Contains);
  NODE_SET_PROTOTYPE_METHOD(tpl, "findAllMatches", FindAllMatches);

  constructor.Reset(isolate, tpl->GetFunction());
  exports->Set(String::NewFromUtf8(isolate, "UnorderedBufferSet"), tpl->GetFunction());
}

void
UnorderedBufferSet::New(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  if (args.IsConstructCall()) {
    // Invoked as constructor: `new MyObject(...)`
    const char* s = node::Buffer::Data(args[0]);
    const size_t len = node::Buffer::Length(args[0]);
    UnorderedBufferSet* obj = new UnorderedBufferSet(s, len);
    obj->Wrap(args.This());
    args.GetReturnValue().Set(args.This());
  } else {
    // Invoked as plain function `MyObject(...)`, turn into construct call
    Local<Value> argv[1] = { args[0] };
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    args.GetReturnValue().Set(cons->NewInstance(1, argv));
  }
}

void UnorderedBufferSet::Contains(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  UnorderedBufferSet* obj = ObjectWrap::Unwrap<UnorderedBufferSet>(args.Holder());

  bool ret = false;

  Local<Value> arg = args[0]; // Buffer or String

  if (node::Buffer::HasInstance(arg)) {
    // It's a buffer. Go char-by-char
    const char* data(node::Buffer::Data(arg));
    const size_t len(node::Buffer::Length(arg));

    ret = obj->contains(data, len);
  } else {
    // We can convert it to utf-8. On failure, it's just an empty String.
    String::Utf8Value argString(arg);
    ret = obj->contains(*argString, argString.length());
  }

  args.GetReturnValue().Set(ret);
}

void
UnorderedBufferSet::FindAllMatches(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  UnorderedBufferSet* obj = ObjectWrap::Unwrap<UnorderedBufferSet>(args.Holder());

  std::vector<std::string> ret;

  Local<Value> arg = args[0]; // Buffer or String
  uint32_t maxNgramSize = args[1]->Uint32Value();
  if (maxNgramSize == 0) maxNgramSize = 1;

  if (node::Buffer::HasInstance(arg)) {
    const char* data(node::Buffer::Data(arg));
    const size_t len(node::Buffer::Length(arg));
    ret = obj->findAllMatches(data, len, maxNgramSize);
  } else {
    // We can convert it to utf-8. On failure, it's just an empty String.
    String::Utf8Value argString(arg);
    ret = obj->findAllMatches(*argString, argString.length(), maxNgramSize);
  }

  size_t size = ret.size();
  Handle<Array> retArray = Array::New(isolate, size);
  for (size_t i = 0; i < size; i++) {
    retArray->Set(i, String::NewFromUtf8(isolate, ret[i].data(), String::NewStringType::kNormalString, ret[i].length()));
  }

  args.GetReturnValue().Set(retArray);
}

void
init(Handle<Object> exports, Handle<Object> module) {
  UnorderedBufferSet::Init(exports);
}

NODE_MODULE(binding, init);
