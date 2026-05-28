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

## Lifecycle & third-party integration guidelines

Use this section when defining SDK or cross-module contracts. Behavior is covered by `test/object_mapper_read_test.cc`.

### Core rules

| Rule | Details |
|------|---------|
| **Keep `Object<T>` for long-lived views** | `ofb::Object<T>` (`FlatBuffersWrapper<T>`) owns or pins the underlying bytes. |
| **Do not cache bare table pointers** | `obj.operator->()`, `monster->name()`, `name()->c_str()`, etc. all point into the **same** buffer; they become invalid when `Object` is destroyed. |
| **Use `UnPack()` for owned data** | `std::unique_ptr<MonsterT>(obj->UnPack())` allocates a native object with copied strings; safe after `Object` is gone. |
| **Writes copy synchronously** | `createDtoResponse` calls `writeToString` before returning; the HTTP body does not reference handler stack locals. |

### HTTP read (`readBodyToDto` / `readBodyToDtoAsync`)

Typical path: `BodyDecoder` → `oatpp::String` body → `readFromString` → `Object<T>`.

- When `Caret` has `getDataMemoryHandle()` (from `oatpp::String`), the mapper **borrows** that storage instead of copying into a `vector`.
- **`Object<T>` must outlive all field access**; destroying it releases the backing `std::string`.
- In async handlers, store **`ofb::Object<T>` by value** (or `UnPack()`), not only `const T*`.

```cpp
// OK: keep the wrapper
ofb::Object<MyGame::Example::Monster> m_monster;

Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monsterObj) {
  m_monster = monsterObj;
  return _return(...);
}

// Unsafe: bare pointer outlives the wrapper
const MyGame::Example::Monster* m_bad = nullptr;
Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monsterObj) {
  m_bad = monsterObj.operator->();
  return _return(...);
}
```

### Local build (`FlatBufferBuilder` / `fromBuffer`)

- After **`FlatBufferBuilder` is destroyed**, `GetBufferPointer()` is invalid. Copy into a `vector` first:

```cpp
flatbuffers::FlatBufferBuilder fbb;
// ... Finish ...
auto buffer = std::make_shared<std::vector<uint8_t>>(
    fbb.GetBufferPointer(),
    fbb.GetBufferPointer() + fbb.GetSize());
auto obj = ofb::Object<MyGame::Example::Monster>::fromBuffer(buffer);
```

- `fromBuffer` uses an **owned** `shared_ptr<const vector<uint8_t>>` (different from HTTP borrow, same rule: keep `Object` or the buffer alive while reading).

### Raw `Caret` and non-HTTP sources

- `Caret(const char*, size)` without a memory handle triggers a **full copy** into `vector`.
- If you pass raw pointers yourself, they must remain valid for the whole lifetime of `Object<T>`, or use `fromBuffer` / `oatpp::String`.

### Mutation

- Only `fromMutableBuffer` + `getMutable()` supports in-buffer mutation; HTTP borrow paths are **read-only** views.
- Whether `mutate_*` exists depends on your schema and FlatBuffers codegen options.

### Async and threads

- Delayed access is fine as long as **`Object<T>`** is still held (see lifecycle tests).
- Cross-thread use follows normal `shared_ptr` rules; no extra locking in this library.

### Validation and typing

- No automatic schema verification; use `flatbuffers::Verifier` for untrusted input if needed.
- `readBodyToDtoAsync<Object<T>>` requires a compile-time `T`; use separate types/branches for multiple roots.

### Running lifecycle tests

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++ \
  -DFlatBuffers_DIR=/usr/lib/x86_64-linux-gnu/cmake/flatbuffers \
  -DOATPP_MODULES_LOCATION=EXTERNAL -DOATPP_BUILD_TESTS=ON
cmake --build build -j4
./build/bin/oatpp_flatbuffers_object_mapper_read_test
# or: ctest --test-dir build -R oatpp_flatbuffers_object_mapper_read_test
```

## License

Apache-2.0.
