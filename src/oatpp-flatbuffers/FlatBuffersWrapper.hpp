/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#ifndef OATPP_FLATBUFFERS_FLATBUFFERS_WRAPPER_HPP
#define OATPP_FLATBUFFERS_FLATBUFFERS_WRAPPER_HPP

#include "oatpp/Types.hpp"
#include "oatpp/data/type/Object.hpp"
#include "oatpp/utils/parser/Caret.hpp"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>

#include "flatbuffers/buffer.h"


namespace oatpp { namespace flatbuffers {
namespace type = oatpp::data::type;

/**
 * 与 oatpp::utils::parser::Caret::getDataMemoryHandle() 相同，用于在零拷贝路径下
 * 延长底层 body 缓冲的生存期。
 */
using CaretMemoryHandle = decltype(std::declval<oatpp::utils::parser::Caret>().getDataMemoryHandle());

/**
 * ObjectMapper::read 提供给类型工厂的缓冲来源：要么拥有 vector 拷贝，要么借用
 * Caret 子区间并持有其内存句柄。
 */
struct FlatBuffersBufferSource {
  std::shared_ptr<const std::vector<uint8_t>> owned;
  CaretMemoryHandle anchor;
  const uint8_t* borrowData = nullptr;
  v_buff_size borrowSize = 0;
};

/**
 * 非模板的抽象基类，统一导出 buffer 访问以便 ObjectMapper 在运行时处理。
 */
class AbstractFlatBuffersObject : public oatpp::data::type::BaseObject {
public:
  class Class {
  public:
    static const oatpp::data::type::ClassId CLASS_ID;
    static oatpp::data::type::Type* getType();
  };
public:
  virtual const uint8_t* getBufferData() const = 0;
  virtual v_buff_size getBufferSize() const = 0;
};

/**
 * 工厂注册表：将具体 `FlatBuffersWrapper<T>` 的类型指针映射到构造器，
 * 以便 ObjectMapper::read() 能根据请求的 Type 创建对应 T 的包装对象。
 */
class FlatBuffersTypeRegistry {
public:
  using Factory = std::function<oatpp::Void(const FlatBuffersBufferSource&)>;
private:
  std::mutex m_mutex;
  std::unordered_map<const oatpp::data::type::Type*, Factory> m_factories;
public:
  static FlatBuffersTypeRegistry& instance() {
    static FlatBuffersTypeRegistry inst;
    return inst;
  }
  void registerFactory(const oatpp::data::type::Type* type, Factory factory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_factories[type] = std::move(factory);
  }
  Factory findFactory(const oatpp::data::type::Type* type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_factories.find(type);
    if (it != m_factories.end()) return it->second;
    return nullptr;
  }
};

/**
 * FlatBuffers 包装类型：持有底层字节缓冲与根表指针，保障生命周期安全。
 * @tparam T - FlatBuffers 生成的 Table 类型
 */
template<typename T>
class FlatBuffersWrapper : public AbstractFlatBuffersObject {
public:
  class Class {
  public:
    static const oatpp::data::type::ClassId CLASS_ID;
    static oatpp::data::type::Type* getType();
  };
private:
  std::shared_ptr<const std::vector<uint8_t>> m_constBuffer;
  std::shared_ptr<std::vector<uint8_t>> m_mutableBuffer;
  CaretMemoryHandle m_borrowHandle;
  const uint8_t* m_borrowData = nullptr;
  v_buff_size m_borrowSize = 0;
  const T* m_constTable = nullptr;
  T* m_mutableTable = nullptr;
public:
  FlatBuffersWrapper(const std::shared_ptr<const std::vector<uint8_t>>& buffer, const T* table)
    : m_constBuffer(buffer)
    , m_constTable(table)
  {}
  FlatBuffersWrapper(const std::shared_ptr<std::vector<uint8_t>>& buffer, T* table)
    : m_mutableBuffer(buffer)
    , m_mutableTable(table)
  {}
  FlatBuffersWrapper(CaretMemoryHandle anchor, const uint8_t* data, v_buff_size size, const T* table)
    : m_borrowHandle(std::move(anchor))
    , m_borrowData(data)
    , m_borrowSize(size)
    , m_constTable(table)
  {}
  const T* getTable() const {
    return m_mutableTable ? m_mutableTable : m_constTable;
  }
  T* getMutableTable() const {
    return m_mutableTable;
  }
  const uint8_t* getBufferData() const override {
    if (m_borrowHandle) return m_borrowData;
    if (m_mutableBuffer) return m_mutableBuffer->data();
    if (m_constBuffer) return m_constBuffer->data();
    return nullptr;
  }
  v_buff_size getBufferSize() const override {
    if (m_borrowHandle) return m_borrowSize;
    if (m_mutableBuffer) return static_cast<v_buff_size>(m_mutableBuffer->size());
    if (m_constBuffer) return static_cast<v_buff_size>(m_constBuffer->size());
    return 0;
  }
  static std::shared_ptr<FlatBuffersWrapper<T>> createShared(
      const std::shared_ptr<const std::vector<uint8_t>>& buffer,
      const T* table) {
    return std::make_shared<FlatBuffersWrapper<T>>(buffer, table);
  }
  static std::shared_ptr<FlatBuffersWrapper<T>> createShared(
      const std::shared_ptr<std::vector<uint8_t>>& buffer,
      T* table) {
    return std::make_shared<FlatBuffersWrapper<T>>(buffer, table);
  }
  static std::shared_ptr<FlatBuffersWrapper<T>> fromSource(const FlatBuffersBufferSource& src) {
    if (src.owned) {
      if (src.owned->empty()) return nullptr;
      const uint8_t* data = src.owned->data();
      const T* table = ::flatbuffers::GetRoot<T>(data);
      return createShared(src.owned, table);
    }
    if (!src.anchor || !src.borrowData || src.borrowSize < 4) {
      return nullptr;
    }
    const T* table = ::flatbuffers::GetRoot<T>(src.borrowData);
    return std::make_shared<FlatBuffersWrapper<T>>(
        CaretMemoryHandle(src.anchor), src.borrowData, src.borrowSize, table);
  }
  static std::shared_ptr<FlatBuffersWrapper<T>> fromBuffer(
      const std::shared_ptr<const std::vector<uint8_t>>& buffer) {
    FlatBuffersBufferSource src;
    src.owned = buffer;
    return fromSource(src);
  }
  static std::shared_ptr<FlatBuffersWrapper<T>> fromMutableBuffer(
      const std::shared_ptr<std::vector<uint8_t>>& buffer) {
    if (!buffer || buffer->empty()) return nullptr;
    uint8_t* data = buffer->data();
    T* table = ::flatbuffers::GetMutableRoot<T>(data);
    return createShared(buffer, table);
  }
};

/**
 * 提供 `oatpp::flatbuffers::Object<T>`，模拟 `oatpp::Object<Dto>` 的易用语法。
 * - operator->() 直接访问生成的 Table 的方法（如 name(), mutate_name()）。
 */
template<typename T>
class Object : public oatpp::data::type::ObjectWrapper<FlatBuffersWrapper<T>, typename FlatBuffersWrapper<T>::Class> {
  using Wrapper = oatpp::data::type::ObjectWrapper<FlatBuffersWrapper<T>, typename FlatBuffersWrapper<T>::Class>;
public:
  OATPP_DEFINE_OBJECT_WRAPPER_DEFAULTS(Object, FlatBuffersWrapper<T>, typename FlatBuffersWrapper<T>::Class)
  const T* operator->() const {
    return this->m_ptr ? this->m_ptr->getTable() : nullptr;
  }
  T* getMutable() const {
    return this->m_ptr ? this->m_ptr->getMutableTable() : nullptr;
  }
  static Object<T> fromBuffer(const std::shared_ptr<const std::vector<uint8_t>>& buffer) {
    return Object<T>(FlatBuffersWrapper<T>::fromBuffer(buffer));
  }
  static Object<T> fromMutableBuffer(const std::shared_ptr<std::vector<uint8_t>>& buffer) {
    return Object<T>(FlatBuffersWrapper<T>::fromMutableBuffer(buffer));
  }
};

// ===== 类型与注册实现 =====

inline const oatpp::data::type::ClassId AbstractFlatBuffersObject::Class::CLASS_ID("flatbuffers::ObjectBase");
inline oatpp::data::type::Type* AbstractFlatBuffersObject::Class::getType() {
  static oatpp::data::type::Type type(CLASS_ID);
  return &type;
}

template<typename T>
inline const oatpp::data::type::ClassId FlatBuffersWrapper<T>::Class::CLASS_ID("flatbuffers::FlatBuffersWrapper");

template<typename T>
inline oatpp::data::type::Type* FlatBuffersWrapper<T>::Class::getType() {
  static oatpp::data::type::Type* type = [](){
    oatpp::data::type::Type::Info info;
    info.parent = AbstractFlatBuffersObject::Class::getType();
    auto* t = new oatpp::data::type::Type(FlatBuffersWrapper<T>::Class::CLASS_ID, info);
    FlatBuffersTypeRegistry::instance().registerFactory(t, [](const FlatBuffersBufferSource& src){
      auto w = FlatBuffersWrapper<T>::fromSource(src);
      return oatpp::Void(w, FlatBuffersWrapper<T>::Class::getType());
    });
    return t;
  }();
  return type;
}

}}

#endif /* OATPP_FLATBUFFERS_FLATBUFFERS_WRAPPER_HPP */
