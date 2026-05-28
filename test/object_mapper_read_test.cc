/**
 * FlatBuffers 读路径生命周期测试
 *
 * 覆盖此前需要人工确认的场景：Caret/oatpp::String 销毁后 Object 是否仍有效、
 * 裸指针 Caret 是否拷贝、fromBuffer 拥有缓冲、UnPack 与 buffer 视图分离等。
 */

#include "oatpp-flatbuffers/ObjectMapper.hpp"
#include "oatpp-flatbuffers/FlatBuffersWrapper.hpp"
#include "oatpp/utils/parser/Caret.hpp"
#include "oatpp/Environment.hpp"
#include "oatpp/Types.hpp"
#include "oatpp/data/stream/BufferStream.hpp"
#include "monster_test_generated.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace ofb = oatpp::flatbuffers;

namespace {

int g_failures = 0;

void require(bool cond, const char* expr, const char* testName) {
  if (!cond) {
    ++g_failures;
    std::cerr << "[FAIL] " << testName << ": " << expr << std::endl;
  }
}

template<typename Fn>
void run_test(const char* name, Fn fn) {
  const int before = g_failures;
  try {
    fn();
    if (g_failures == before) {
      std::cout << "[PASS] " << name << std::endl;
    } else {
      std::cerr << "[FAIL] " << name << " (" << (g_failures - before) << " assertion(s))" << std::endl;
    }
  } catch (const std::exception& e) {
    ++g_failures;
    std::cerr << "[FAIL] " << name << ": exception: " << e.what() << std::endl;
  }
}

std::shared_ptr<std::vector<uint8_t>> buildMinimalMonsterBytes() {
  flatbuffers::FlatBufferBuilder builder(256);
  auto name = builder.CreateString("lifecycle-test");
  MyGame::Example::MonsterBuilder mb(builder);
  mb.add_name(name);
  mb.add_hp(42);
  mb.add_mana(7);
  auto root = mb.Finish();
  builder.Finish(root);
  return std::make_shared<std::vector<uint8_t>>(
      builder.GetBufferPointer(),
      builder.GetBufferPointer() + builder.GetSize());
}

oatpp::String bytesToOatppString(const std::shared_ptr<std::vector<uint8_t>>& bytes) {
  return oatpp::String(reinterpret_cast<const char*>(bytes->data()),
                       static_cast<v_buff_size>(bytes->size()));
}

void assertMonsterFields(const ofb::Object<MyGame::Example::Monster>& monster,
                         const char* testName) {
  require(static_cast<bool>(monster), "monster non-null", testName);
  require(monster->name() != nullptr, "name non-null", testName);
  require(monster->hp() == 42, "hp == 42", testName);
  require(monster->mana() == 7, "mana == 7", testName);
  if (monster->name()) {
    require(std::strcmp(monster->name()->c_str(), "lifecycle-test") == 0,
            "name string content",
            testName);
  }
}

std::shared_ptr<ofb::ObjectMapper> makeMapper() {
  return std::make_shared<ofb::ObjectMapper>();
}

// 模拟 readBodyToDto：body 为独立 oatpp::String，再 readFromString
ofb::Object<MyGame::Example::Monster> parseLikeHttpBody(
    const std::shared_ptr<ofb::ObjectMapper>& mapper,
    const oatpp::String& body) {
  return mapper->readFromString<ofb::Object<MyGame::Example::Monster>>(body);
}

ofb::Object<MyGame::Example::Monster> parseInInnerScopeWithStringBody() {
  auto mapper = makeMapper();
  auto bytes = buildMinimalMonsterBytes();
  oatpp::String body = bytesToOatppString(bytes);
  bytes.reset();
  return parseLikeHttpBody(mapper, body);
}

ofb::Object<MyGame::Example::Monster> parseInInnerScopeWithCaret() {
  auto mapper = makeMapper();
  auto bytes = buildMinimalMonsterBytes();
  oatpp::String body = bytesToOatppString(bytes);
  bytes.reset();
  oatpp::utils::parser::Caret caret(body);
  body = nullptr;
  return mapper->readFromCaret<ofb::Object<MyGame::Example::Monster>>(caret);
}

void test_readFromString_same_scope() {
  auto mapper = makeMapper();
  auto bytes = buildMinimalMonsterBytes();
  auto monster = parseLikeHttpBody(mapper, bytesToOatppString(bytes));
  assertMonsterFields(monster, "readFromString_same_scope");
}

void test_readFromString_body_and_source_vector_destroyed_before_use() {
  ofb::Object<MyGame::Example::Monster> monster = parseInInnerScopeWithStringBody();
  assertMonsterFields(monster, "body_and_vector_destroyed");
}

void test_readFromCaret_body_destroyed_before_use() {
  ofb::Object<MyGame::Example::Monster> monster = parseInInnerScopeWithCaret();
  assertMonsterFields(monster, "caret_body_destroyed");
}

void test_delayed_access_after_parse() {
  ofb::Object<MyGame::Example::Monster> monster = parseInInnerScopeWithStringBody();
  for (int i = 0; i < 1000; ++i) {
    (void)i;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  assertMonsterFields(monster, "delayed_access");
}

void test_name_cstr_after_body_gone() {
  ofb::Object<MyGame::Example::Monster> monster = parseInInnerScopeWithStringBody();
  const char* nameCstr = monster->name() ? monster->name()->c_str() : nullptr;
  require(nameCstr != nullptr, "name cstr non-null", "name_cstr_after_body_gone");
  require(std::strlen(nameCstr) > 0, "name cstr non-empty", "name_cstr_after_body_gone");
  require(std::strcmp(nameCstr, "lifecycle-test") == 0,
          "name cstr content",
          "name_cstr_after_body_gone");
}

void test_raw_caret_copies_underlying() {
  auto mapper = makeMapper();
  auto bytes = buildMinimalMonsterBytes();
  std::vector<uint8_t> local = *bytes;
  oatpp::utils::parser::Caret caret(
      reinterpret_cast<const char*>(local.data()),
      static_cast<v_buff_size>(local.size()));
  auto monster = mapper->readFromCaret<ofb::Object<MyGame::Example::Monster>>(caret);
  assertMonsterFields(monster, "raw_caret_before_clear");
  std::memset(local.data(), 0, local.size());
  assertMonsterFields(monster, "raw_caret_after_clear");
}

void test_fromBuffer_outlives_local_vector_shared_ptr() {
  ofb::Object<MyGame::Example::Monster> monster;
  {
    auto bytes = buildMinimalMonsterBytes();
    monster = ofb::Object<MyGame::Example::Monster>::fromBuffer(bytes);
  }
  assertMonsterFields(monster, "fromBuffer_outlives_vector");
}

void test_fromBuffer_keeps_bytes_when_only_object_remains() {
  ofb::Object<MyGame::Example::Monster> monster;
  {
    auto bytes = buildMinimalMonsterBytes();
    monster = ofb::Object<MyGame::Example::Monster>::fromBuffer(bytes);
  }
  assertMonsterFields(monster, "fromBuffer_only_object_remains");
}

void test_assign_object_to_outer_storage() {
  ofb::Object<MyGame::Example::Monster> stored;
  {
    auto inner = parseInInnerScopeWithStringBody();
    stored = inner;
  }
  assertMonsterFields(stored, "assign_to_outer");
}

void test_unpacked_native_survives_after_object_destroyed() {
  std::unique_ptr<MyGame::Example::MonsterT> native;
  {
    auto monster = parseInInnerScopeWithStringBody();
    assertMonsterFields(monster, "before_unpack");
    native.reset(monster->UnPack());
  }
  require(native != nullptr, "native non-null", "unpacked_survives");
  require(native->hp == 42, "native hp", "unpacked_survives");
  require(native->name == "lifecycle-test", "native name", "unpacked_survives");
}

void test_builder_copy_vs_dangling_pattern_documented() {
  std::shared_ptr<std::vector<uint8_t>> owned;
  {
    flatbuffers::FlatBufferBuilder builder(128);
    auto n = builder.CreateString("builder");
    MyGame::Example::MonsterBuilder mb(builder);
    mb.add_name(n);
    mb.add_hp(1);
    auto root = mb.Finish();
    builder.Finish(root);
    owned = std::make_shared<std::vector<uint8_t>>(
        builder.GetBufferPointer(),
        builder.GetBufferPointer() + builder.GetSize());
  }
  auto monster = ofb::Object<MyGame::Example::Monster>::fromBuffer(owned);
  require(static_cast<bool>(monster), "from builder copy", "builder_copy");
  require(monster->hp() == 1, "builder copy hp", "builder_copy");
}

void test_http_body_string_independent_of_decode_buffer_stream() {
  auto mapper = makeMapper();
  oatpp::data::stream::BufferOutputStream stream;
  auto bytes = buildMinimalMonsterBytes();
  stream.writeSimple(bytes->data(), static_cast<v_buff_size>(bytes->size()));
  oatpp::String body = stream.toString();
  bytes.reset();
  auto monster = mapper->readFromString<ofb::Object<MyGame::Example::Monster>>(body);
  body = nullptr;
  assertMonsterFields(monster, "buffer_stream_body_independent");
}

} // namespace

int main() {
  oatpp::Environment::init();

  run_test("readFromString_same_scope", test_readFromString_same_scope);
  run_test("readFromString_body_and_source_vector_destroyed_before_use",
           test_readFromString_body_and_source_vector_destroyed_before_use);
  run_test("readFromCaret_body_destroyed_before_use", test_readFromCaret_body_destroyed_before_use);
  run_test("delayed_access_after_parse", test_delayed_access_after_parse);
  run_test("name_cstr_after_body_gone", test_name_cstr_after_body_gone);
  run_test("raw_caret_copies_underlying", test_raw_caret_copies_underlying);
  run_test("fromBuffer_outlives_local_vector_shared_ptr", test_fromBuffer_outlives_local_vector_shared_ptr);
  run_test("fromBuffer_keeps_bytes_when_only_object_remains", test_fromBuffer_keeps_bytes_when_only_object_remains);
  run_test("assign_object_to_outer_storage", test_assign_object_to_outer_storage);
  run_test("unpacked_native_survives_after_object_destroyed", test_unpacked_native_survives_after_object_destroyed);
  run_test("builder_copy_vs_dangling_pattern_documented", test_builder_copy_vs_dangling_pattern_documented);
  run_test("http_body_string_independent_of_decode_buffer_stream",
           test_http_body_string_independent_of_decode_buffer_stream);

  oatpp::Environment::destroy();

  if (g_failures > 0) {
    std::cerr << g_failures << " test failure(s)" << std::endl;
    return 1;
  }
  std::cout << "All lifecycle tests passed." << std::endl;
  return 0;
}
