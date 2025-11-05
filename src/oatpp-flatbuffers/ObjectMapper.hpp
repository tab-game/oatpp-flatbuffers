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

#ifndef OATPP_FLATBUFFERS_OBJECTMAPPER_HPP
#define OATPP_FLATBUFFERS_OBJECTMAPPER_HPP

#include "FlatBuffersWrapper.hpp"

#include "oatpp/data/mapping/ObjectMapper.hpp"
#include "oatpp/data/stream/Stream.hpp"
#include "oatpp/utils/parser/Caret.hpp"
#include "oatpp/Types.hpp"

namespace oatpp { namespace flatbuffers {

/**
 * FlatBuffers ObjectMapper.
 * Serializes/Deserializes flatbuffers objects to/from binary streams.
 * 
 * Unlike oatpp-bob and oatpp::json::ObjectMapper, this mapper does NOT use
 * TreeToObjectMapper or ObjectToTreeMapper, as flatbuffers binary data doesn't
 * need an intermediate tree structure. It directly uses flatbuffers native APIs.
 * 
 * Extends &id:oatpp::base::Countable;, &id:oatpp::data::mapping::ObjectMapper;.
 */
class ObjectMapper : public oatpp::base::Countable, public oatpp::data::mapping::ObjectMapper {
private:
  static Info getMapperInfo() {
    return Info("application", "x-flatbuffers");
  }

public:
  
  /**
   * Constructor.
   */
  ObjectMapper();
  
  /**
   * Serialize object to stream.
   * @param stream - &id:oatpp::data::stream::ConsistentOutputStream; to serialize object to.
   * @param variant - Object to serialize (should be FlatBuffersWrapper or raw flatbuffers Table pointer).
   * @param errorStack - See &id:oatpp::data::mapping::ErrorStack;.
   */
  void write(data::stream::ConsistentOutputStream* stream, 
             const oatpp::Void& variant, 
             data::mapping::ErrorStack& errorStack) const override;
  
  /**
   * Deserialize object from stream.
   * @param caret - &id:oatpp::utils::parser::Caret; over serialized buffer.
   * @param type - pointer to object type. See &id:oatpp::data::type::Type;.
   * @param errorStack - See &id:oatpp::data::mapping::ErrorStack;.
   * @return - deserialized object wrapped in &id:oatpp::Void;.
   */
  oatpp::Void read(oatpp::utils::parser::Caret& caret, 
                   const oatpp::Type* type, 
                   data::mapping::ErrorStack& errorStack) const override;

private:
  
  /**
   * Helper method to write flatbuffers binary data to stream.
   * @param stream - Output stream.
   * @param data - Pointer to flatbuffers binary data.
   * @param size - Size of the data in bytes.
   * @param errorStack - Error stack.
   */
  void writeBinaryData(data::stream::ConsistentOutputStream* stream,
                       const void* data,
                       v_buff_size size,
                       data::mapping::ErrorStack& errorStack) const;
  
  /**
   * Helper method to read flatbuffers binary data from caret.
   * @param caret - Input caret.
   * @param type - Expected type.
   * @param errorStack - Error stack.
   * @return - FlatBuffersWrapper object or nullptr on error.
   */
  oatpp::Void readBinaryData(oatpp::utils::parser::Caret& caret,
                              const oatpp::Type* type,
                              data::mapping::ErrorStack& errorStack) const;
};

}}

#endif /* OATPP_FLATBUFFERS_OBJECTMAPPER_HPP */

