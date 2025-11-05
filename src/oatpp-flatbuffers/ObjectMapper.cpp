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

#include "ObjectMapper.hpp"

#include "FlatBuffersWrapper.hpp"
#include "flatbuffers/base.h"
#include "flatbuffers/verifier.h"

#include <vector>
#include <memory>

namespace oatpp { namespace flatbuffers {

ObjectMapper::ObjectMapper()
  : data::mapping::ObjectMapper(getMapperInfo())
{}

void ObjectMapper::writeBinaryData(data::stream::ConsistentOutputStream* stream,
                                   const void* data,
                                   v_buff_size size,
                                   data::mapping::ErrorStack& errorStack) const {
  
  if (!data || size == 0) {
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::writeBinaryData()]: Invalid data or size");
    return;
  }
  
  v_io_size written = stream->writeSimple(data, size);
  if (written != size) {
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::writeBinaryData()]: Failed to write all data");
    return;
  }
}

void ObjectMapper::write(data::stream::ConsistentOutputStream* stream,
                         const oatpp::Void& variant,
                         data::mapping::ErrorStack& errorStack) const {
  
  if (!variant) {
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::write()]: Variant is null");
    return;
  }
  
  // 优先：FlatBuffers 包装对象（任意 T），类型应当继承自 AbstractFlatBuffersObject。
  const auto* vt = variant.getValueType();
  if (vt && vt->extends(AbstractFlatBuffersObject::Class::getType())) {
    // 注意：oatpp 的 ObjectWrapper::cast 要求“目标类型 extends 源类型”，
    // 这里我们做上行转换（子 -> 父），因此不能用 cast。改为通过别名 shared_ptr 获取基类指针。
    auto raw = static_cast<const AbstractFlatBuffersObject*>(variant.get());
    if (raw) {
      const uint8_t* data = raw->getBufferData();
      v_buff_size size = raw->getBufferSize();
      if (data && size > 0) {
        writeBinaryData(stream, data, size, errorStack);
        return;
      }
    }
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::write()]: Empty flatbuffers buffer");
    return;
  }
  
  // 兼容：直接传 shared_ptr<std::vector<uint8_t>> 的情况
  auto ptr = variant.getPtr();
  if (ptr) {
    const auto* buffer = static_cast<const std::vector<uint8_t>*>(ptr.get());
    if (buffer && !buffer->empty()) {
      writeBinaryData(stream, buffer->data(), static_cast<v_buff_size>(buffer->size()), errorStack);
      return;
    }
  }
  
  errorStack.push("[oatpp::flatbuffers::ObjectMapper::write()]: Unsupported variant type for flatbuffers serialization");
}

oatpp::Void ObjectMapper::read(oatpp::utils::parser::Caret& caret,
                                const oatpp::Type* type,
                                data::mapping::ErrorStack& errorStack) const {
  
  if (!type) {
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::read()]: Type is null");
    return nullptr;
  }
  
  // Get remaining data from caret
  const char* data = caret.getData();
  v_buff_size totalSize = caret.getDataSize();
  v_buff_size position = caret.getPosition();
  
  if (totalSize == 0 || position >= totalSize) {
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::read()]: No data available");
    return nullptr;
  }
  
  // Calculate available data size
  v_buff_size available = totalSize - position;
  
  // For flatbuffers, we need at least 4 bytes (root offset)
  if (available < 4) {
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::read()]: Buffer too small (minimum 4 bytes)");
    return nullptr;
  }
  
  // Get the buffer data
  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(data + position);
  
  // For flatbuffers, we need to determine the actual buffer size
  // FlatBuffers buffers can have:
  // 1. Size prefix (4 bytes) + data
  // 2. Just data (no size prefix)
  
  v_buff_size bufferSize = available;
  
  // Try to read size prefix if present
  // FlatBuffers may use size-prefixed buffers
  if (available >= 4) {
    // Check if first 4 bytes look like a size prefix
    // Size prefix is typically the size of the buffer (excluding the prefix itself)
    v_uint32 sizePrefix = ::flatbuffers::ReadScalar<v_uint32>(buffer);
    if (sizePrefix > 0 && sizePrefix < available && sizePrefix + 4 <= available) {
      // This might be a size prefix
      // But we need to verify it's actually a flatbuffers buffer
      // For now, we'll use the full available size
      // In practice, you might want to verify the buffer structure
    }
  }
  
  // Verify the buffer is a valid flatbuffers buffer
  // Use flatbuffers::Verifier to check buffer validity
  ::flatbuffers::Verifier verifier(buffer, bufferSize);
  // Note: We don't verify here because we don't know the root table type
  // Verification should be done by the user when they call GetRoot<T>
  
  // Create a shared_ptr to hold the buffer
  // We need to copy the buffer data to ensure it remains valid
  auto bufferCopy = std::make_shared<std::vector<uint8_t>>(buffer, buffer + bufferSize);
  
  // Update caret position
  caret.setPosition(position + bufferSize);
  
  // 如果用户请求的是 flatbuffers 对象类型（继承自 AbstractFlatBuffersObject），
  // 则根据类型注册表构造相应 T 的包装对象；否则退回为字节数组。
  if (type->extends(AbstractFlatBuffersObject::Class::getType())) {
    auto factory = FlatBuffersTypeRegistry::instance().findFactory(type);
    if (factory) {
      return factory(bufferCopy);
    }
    errorStack.push("[oatpp::flatbuffers::ObjectMapper::read()]: No factory registered for requested FlatBuffers type");
    return nullptr;
  }
  
  return oatpp::Void(bufferCopy);
}

}}

