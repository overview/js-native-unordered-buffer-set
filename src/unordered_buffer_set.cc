#include <cstdlib>
#include <cstring>
#include <deque>
#include <unordered_set>
#include <vector>

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <v8.h>

#include "farmhash.h"

using namespace v8;

// It turns out std::string's memory allocations are the bottleneck. Nix them
// all by using a copy of the original input as the actual data structure.
//
// In other words: SimpleString holds a pointer that must be managed by its
// caller.
struct PooledString {
  const char* start; // Points to UnorderedBufferSet.mem
  size_t length;

  bool operator==(const PooledString& rhs) const {
    return rhs.length == this->length
      && memcmp(this->start, rhs.start, this->length) == 0;
  }

  PooledString& operator=(const PooledString& rhs) {
    this->start = rhs.start;
    this->length = rhs.length;
    return *this;
  }

  explicit PooledString(): start(NULL), length(-1) {}
  explicit PooledString(const PooledString& rhs): start(rhs.start), length(rhs.length) {}
  explicit PooledString(const char* s, size_t l): start(s), length(l) {}
};

namespace std
{
  template<> struct hash<PooledString> {
    size_t operator()(const PooledString& str) const {
      return util::Fingerprint64(str.start, str.length);
    }
  };
}

class UnorderedBufferSet : public node::ObjectWrap {
public:
  static void Init(Handle<Object> exports);
  static Persistent<Function> constructor;

  inline bool contains(const char* s, size_t len);
  std::vector<PooledString> findAllMatches(const char* s, size_t len, size_t maxNgramSize);

private:
  std::unordered_set<PooledString> set;
  char* mem = NULL;

  explicit UnorderedBufferSet(const char* s, size_t len);
  ~UnorderedBufferSet();

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Contains(const FunctionCallbackInfo<Value>& args);
  static void FindAllMatches(const FunctionCallbackInfo<Value>& args);
};

Persistent<Function> UnorderedBufferSet::constructor;

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

UnorderedBufferSet::UnorderedBufferSet(const char* s, size_t len)
{
  this->mem = new char[len];
  memcpy(this->mem, s, len);

  this->set.reserve(count_char_in_str('\n', this->mem, len) + 1);

  const char* tokenStart = this->mem;
  const char* end = this->mem + len;

  for (const char* p = tokenStart; p < end; p++) {
    if (*p == '\n') {
      this->set.insert(PooledString(tokenStart, p - tokenStart));
      tokenStart = p + 1;
    }
  }

  if (tokenStart < end) {
    this->set.insert(PooledString(tokenStart, end - tokenStart));
  }
}

UnorderedBufferSet::~UnorderedBufferSet()
{
  if (this->mem) free(this->mem);
}

bool
UnorderedBufferSet::contains(const char* s, size_t len)
{
  const PooledString str(s, len);
  return this->set.count(str) != 0;
}

std::vector<PooledString>
UnorderedBufferSet::findAllMatches(const char* s, size_t len, size_t maxNgramSize) {
  std::vector<PooledString> ret;
  std::deque<const char*> tokenStarts;
  const char* lastP = s;
  const char* end = s + len;

  tokenStarts.push_back(s);

  while (true) {
    const char* p = static_cast<const char*>(memchr(lastP, ' ', end - lastP));
    if (p == NULL) p = s + len;

    // Add s[tokenStarts[0],pos), s[tokenStarts[1],pos), ... for every token
    // in the set
    for (auto i = tokenStarts.begin(); i < tokenStarts.end(); i++) {
      const char* tokenStart = *i;
      PooledString needle(tokenStart, p - tokenStart);
      if (this->set.count(needle) != 0) {
        ret.push_back(needle);
      }
    }

    if (tokenStarts.size() == maxNgramSize) tokenStarts.pop_front();

    if (p == s + len) break;

    lastP = p + 1;
    tokenStarts.push_back(lastP);
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

  std::vector<PooledString> ret;
  Handle<Array> retArray;

  Local<Value> arg = args[0]; // Buffer or String
  uint32_t maxNgramSize = args[1]->Uint32Value();
  if (maxNgramSize == 0) maxNgramSize = 1;

  if (node::Buffer::HasInstance(arg)) {
    const char* data(node::Buffer::Data(arg));
    const size_t len(node::Buffer::Length(arg));
    ret = obj->findAllMatches(data, len, maxNgramSize);

    // Icky copy/paste, I know
    size_t size = ret.size();
    retArray = Array::New(isolate, size);
    for (size_t i = 0; i < size; i++) {
      retArray->Set(i, String::NewFromUtf8(isolate, ret[i].start, String::NewStringType::kNormalString, ret[i].length));
    }
  } else {
    // We can convert it to utf-8. On failure, it's just an empty String.
    String::Utf8Value argString(arg);
    ret = obj->findAllMatches(*argString, argString.length(), maxNgramSize);

    // Icky copy/paste, I know
    size_t size = ret.size();
    retArray = Array::New(isolate, size);
    for (size_t i = 0; i < size; i++) {
      retArray->Set(i, String::NewFromUtf8(isolate, ret[i].start, String::NewStringType::kNormalString, ret[i].length));
    }
  }

  args.GetReturnValue().Set(retArray);
}

void
init(Handle<Object> exports, Handle<Object> module) {
  UnorderedBufferSet::Init(exports);
}

NODE_MODULE(binding, init);
