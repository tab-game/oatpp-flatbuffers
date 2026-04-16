#include "oatpp-flatbuffers/ObjectMapper.hpp"
#include "oatpp-flatbuffers/FlatBuffersWrapper.hpp"
#include "oatpp/utils/parser/Caret.hpp"
#include "oatpp/Types.hpp"
#include "monster_test_generated.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace ofb = oatpp::flatbuffers;

static std::shared_ptr<std::vector<uint8_t>> buildMinimalMonster() {
  flatbuffers::FlatBufferBuilder builder(256);
  auto name = builder.CreateString("T");
  MyGame::Example::MonsterBuilder mb(builder);
  mb.add_name(name);
  mb.add_hp(42);
  mb.add_mana(7);
  auto root = mb.Finish();
  builder.Finish(root);
  auto buf = std::make_shared<std::vector<uint8_t>>(
      builder.GetBufferPointer(),
      builder.GetBufferPointer() + builder.GetSize());
  return buf;
}

static void test_read_with_caret_memory_handle() {
  auto mapper = std::make_shared<ofb::ObjectMapper>();
  auto raw = buildMinimalMonster();
  oatpp::String body(reinterpret_cast<const char*>(raw->data()),
                     static_cast<v_buff_size>(raw->size()));
  oatpp::utils::parser::Caret caret(body);
  auto monster = mapper->readFromCaret<ofb::Object<MyGame::Example::Monster>>(caret);
  if (!monster || !monster->name() || monster->hp() != 42) {
    throw std::runtime_error("readFromCaret with oatpp::String failed");
  }
}

static void test_read_raw_caret_copies_underlying() {
  auto mapper = std::make_shared<ofb::ObjectMapper>();
  auto raw = buildMinimalMonster();
  std::vector<uint8_t> bytes = *raw;
  oatpp::utils::parser::Caret caret(
      reinterpret_cast<const char*>(bytes.data()),
      static_cast<v_buff_size>(bytes.size()));
  auto monster = mapper->readFromCaret<ofb::Object<MyGame::Example::Monster>>(caret);
  if (!monster || monster->hp() != 42) {
    throw std::runtime_error("readFromCaret raw failed before mutation");
  }
  std::memset(bytes.data(), 0, bytes.size());
  if (monster->hp() != 42) {
    throw std::runtime_error("expected copy path: monster invalid after clearing source bytes");
  }
}

int main() {
  test_read_with_caret_memory_handle();
  test_read_raw_caret_copies_underlying();
  return 0;
}
