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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#ifndef OATPP_FLATBUFFERS_FLATBUFFERS_WRAPPER_HPP
#define OATPP_FLATBUFFERS_FLATBUFFERS_WRAPPER_HPP

#include "oatpp/Types.hpp"
#include "oatpp/data/type/Object.hpp"
#include "flatbuffers/flatbuffers.h"

#include <memory>
#include <vector>

namespace oatpp { namespace flatbuffers {

/**
 * FlatBuffers wrapper class that holds both the flatbuffers Table pointer
 * and the underlying binary buffer to ensure the Table remains valid.
 * 
 * This class is necessary because flatbuffers Table pointers are only valid
 * as long as the underlying buffer exists. We need to maintain ownership
 * of the buffer to prevent the Table from becoming invalid.
 * 
 * @tparam T - FlatBuffers Table type (e.g., MyGame::Monster)
 */
template<typename T>
class FlatBuffersWrapper : public oatpp::data::type::BaseObject {
public:
  
  /**
   * Class to provide type information for oatpp reflection system.
   */
  class Class {
  public:
    static const oatpp::data::type::ClassId CLASS_ID;
    
    /**
     * Get class type information.
     * @return - &id:oatpp::data::type::Type;
     */
    static oatpp::data::type::Type* getType();
  };
  
private:
  std::shared_ptr<const void> m_buffer;  // Owning the buffer
  const T* m_table;                        // Non-owning pointer to the Table
  
public:
  
  /**
   * Constructor.
   * @param buffer - Shared pointer to the buffer containing the flatbuffers data.
   * @param table - Pointer to the flatbuffers Table (must be valid as long as buffer exists).
   */
  FlatBuffersWrapper(const std::shared_ptr<const void>& buffer, const T* table)
    : m_buffer(buffer)
    , m_table(table)
  {}
  
  /**
   * Get the flatbuffers Table pointer.
   * @return - Pointer to the flatbuffers Table.
   */
  const T* getTable() const {
    return m_table;
  }
  
  /**
   * Get the underlying buffer.
   * @return - Shared pointer to the buffer.
   */
  std::shared_ptr<const void> getBuffer() const {
    return m_buffer;
  }
  
  /**
   * Get raw pointer to the buffer data.
   * @return - Raw pointer to the buffer data.
   */
  const uint8_t* getBufferData() const {
    return static_cast<const uint8_t*>(m_buffer.get());
  }
  
  /**
   * Get buffer size.
   * @return - Size of the buffer in bytes.
   */
  v_buff_size getBufferSize() const {
    if (!m_buffer) {
      return 0;
    }
    // For flatbuffers, we need to verify the buffer size
    // This is a simplified implementation - in practice, you might need
    // to track the actual size separately
    // Try to get the size from the buffer
    const uint8_t* data = getBufferData();
    if (data) {
      // Check if it's a size-prefixed buffer
      v_uint32 sizePrefix = ::flatbuffers::ReadScalar<v_uint32>(data);
      if (sizePrefix > 0 && sizePrefix < 1024 * 1024 * 1024) {  // Reasonable size limit
        return sizePrefix + 4;  // Size prefix + data
      }
      // Otherwise, we can't determine the size without additional information
      // Users should provide the size separately or use a buffer with known size
    }
    return 0;
  }
  
  /**
   * Create a shared_ptr to FlatBuffersWrapper.
   * @param buffer - Shared pointer to the buffer.
   * @param table - Pointer to the flatbuffers Table.
   * @return - Shared pointer to FlatBuffersWrapper.
   */
  static std::shared_ptr<FlatBuffersWrapper<T>> createShared(
      const std::shared_ptr<const void>& buffer,
      const T* table) {
    return std::make_shared<FlatBuffersWrapper<T>>(buffer, table);
  }
  
  /**
   * Create a FlatBuffersWrapper from a buffer.
   * This helper function extracts the Table from the buffer using GetRoot.
   * @param buffer - Shared pointer to the buffer containing flatbuffers data.
   * @return - Shared pointer to FlatBuffersWrapper.
   */
  static std::shared_ptr<FlatBuffersWrapper<T>> fromBuffer(
      const std::shared_ptr<const std::vector<uint8_t>>& buffer) {
    if (!buffer || buffer->empty()) {
      return nullptr;
    }
    const uint8_t* data = buffer->data();
    const T* table = ::flatbuffers::GetRoot<T>(data);
    return createShared(buffer, table);
  }
  
};

template<typename T>
const oatpp::data::type::ClassId FlatBuffersWrapper<T>::Class::CLASS_ID("flatbuffers::FlatBuffersWrapper");

template<typename T>
oatpp::data::type::Type* FlatBuffersWrapper<T>::Class::getType() {
  static oatpp::data::type::Type type(FlatBuffersWrapper<T>::Class::CLASS_ID);
  return &type;
}

}}

#endif /* OATPP_FLATBUFFERS_FLATBUFFERS_WRAPPER_HPP */

