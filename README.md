# oatpp-flatbuffers

Fast FlatBuffers binary integration for oatpp. It provides an ObjectMapper and typed wrappers so you can read/write FlatBuffers payloads directly in oatpp endpoints and clients with ergonomics similar to `oatpp::Object<Dto>`.

- Content type: `application/x-flatbuffers`
- Typed wrapper: `oatpp::flatbuffers::Object<T>` where `T` is a FlatBuffers generated Table (e.g. `MyGame::Example::Monster`)
- Direct member access: `obj->name()`, `obj->mutate_name(...)`

See also: Chinese version of this README is available as `README.zh-CN.md`.

## Features

- ObjectMapper that serializes/deserializes FlatBuffers without intermediate trees
- `oatpp::flatbuffers::Object<T>` that mirrors `oatpp::Object<Dto>` ergonomics
- Server: return `Object<T>` from `createDtoResponse`
- Client/Server: parse body to `Object<T>` via `readBodyToDtoAsync<Object<T>>(mapper)`
- Backward-compatible: you can still pass `std::shared_ptr<std::vector<uint8_t>>` to send raw bytes

## Install

Prerequisites:
- CMake 3.15+
- A C++17 compiler
- oatpp (link in your environment/include and libs)
- FlatBuffers library and headers

Typical build (Windows example):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target install
```

The library installs a static library and public headers under your CMake install prefix.

## Quick Start

### 1) Create ObjectMapper

```cpp
namespace ofb = oatpp::flatbuffers;

auto flatbuffersMapper = std::make_shared<ofb::ObjectMapper>();
```

You can register it as default in `oatpp::web::mime::ContentMappers` and use it across server/client.

### 2) Server: return a FlatBuffers object

```cpp
// Build a FlatBuffers buffer for Monster
auto buffer = std::make_shared<std::vector<uint8_t>>(builder.GetBufferPointer(),
                                                     builder.GetBufferPointer() + builder.GetSize());
// Wrap as typed Object<Monster>
auto monsterObj = ofb::Object<MyGame::Example::Monster>::fromBuffer(buffer);
return createDtoResponse(Status::CODE_200, monsterObj);
```

The ObjectMapper will write the underlying binary buffer to the HTTP response with content type `application/x-flatbuffers`.

### 3) Server: receive a FlatBuffers object

```cpp
return request->readBodyToDtoAsync<ofb::Object<MyGame::Example::Monster>>(controller->getContentMappers()->getDefaultMapper())
  .callbackTo(&EndpointCoroutine::onMonsterRead);

Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monsterObj) {
  const auto* monster = monsterObj.operator->();
  // Access fields
  auto name = monster->name();
  ...
  return _return(controller->createResponse(Status::CODE_200, "OK"));
}
```

### 4) Client: parse response to `Object<T>`

```cpp
return response->readBodyToDtoAsync<ofb::Object<MyGame::Example::Monster>>(client->getObjectMapper())
  .callbackTo(&ClientCoroutine::onGetMonsterObj);
```

### 5) Mutable access (optional)

If you need `mutate_*` methods, build from a mutable buffer:

```cpp
auto mutableBuf = std::make_shared<std::vector<uint8_t>>(...);
auto obj = ofb::Object<MyGame::Example::Monster>::fromMutableBuffer(mutableBuf);
auto* m = obj.getMutable();
if (m) m->mutate_hp(100);
```

Note: FlatBuffers mutation only works on buffers created with appropriate options and not all fields are mutable. Consider the FlatBuffers schema and generated code constraints.

## Content Type and Negotiation

- Mapper info is registered as vendor type `application/x-flatbuffers`.
- When using `ContentMappers`, set this mapper as default or negotiate via `Accept`/`Content-Type` headers.

## API Overview

- `oatpp::flatbuffers::ObjectMapper` implements `write`/`read` to stream bytes directly.
- `oatpp::flatbuffers::Object<T>` holds a `FlatBuffersWrapper<T>` which keeps the buffer alive and exposes `T*`/`const T*` for member calls.
- Raw bytes compatibility: passing `std::shared_ptr<std::vector<uint8_t>>` as `oatpp::Void` still works for writing.

## Examples

This repository includes minimal async server/client demos under `test/server` and `test/client` that exchange `Monster` objects defined in `monster_test.fbs`.

Build binaries are produced under `build/bin` (e.g., `oatpp_flatbuffers_server.exe`, `oatpp_flatbuffers_client.exe`).

## Notes & Caveats

- Validation: The mapper does not auto-verify buffers by schema. You may use `flatbuffers::Verifier` where appropriate.
- Type binding: `readBodyToDtoAsync<Object<T>>` requires the target `T` to be known at compile-time. The mapper uses an internal registry to construct the right wrapper for `T`.
- Memory: The wrapper retains the underlying buffer (`std::vector<uint8_t>`). Keep this in mind when copying.

## License

Apache-2.0.
