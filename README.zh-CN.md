# oatpp-flatbuffers

为 oatpp 提供高性能 FlatBuffers 二进制集成。包含一个 ObjectMapper 与类型化包装器，允许在服务端/客户端端点中直接读写 FlatBuffers，有着类似 `oatpp::Object<Dto>` 的使用体验。

- 内容类型：`application/x-flatbuffers`
- 类型包装：`oatpp::flatbuffers::Object<T>`，其中 `T` 为 FlatBuffers 生成的 Table（例如 `MyGame::Example::Monster`）
- 直接访问：`obj->name()`、`obj->mutate_name(...)`

英文版请参阅 `README.md`。

## 特性

- 直接面向字节流的 ObjectMapper，不需要中间树结构
- `oatpp::flatbuffers::Object<T>` 提供与 `oatpp::Object<Dto>` 相似的人体工学
- 服务端：可在 `createDtoResponse` 中直接返回 `Object<T>`
- 客户端/服务端：`readBodyToDtoAsync<Object<T>>(mapper)` 直接解析请求/响应体
- 向后兼容：仍可传递 `std::shared_ptr<std::vector<uint8_t>>` 作为原始字节

## 安装

前置依赖：
- CMake 3.15+
- C++17 编译器
- oatpp（需在环境中链接头文件与库）
- FlatBuffers 库与头文件

常规构建（Windows 示例）：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target install
```

安装后会在安装前缀下生成静态库与公共头文件。

## 快速开始

### 1) 创建 ObjectMapper

```cpp
namespace ofb = oatpp::flatbuffers;

auto flatbuffersMapper = std::make_shared<ofb::ObjectMapper>();
```

建议将其注册到 `oatpp::web::mime::ContentMappers` 中，并设置为默认 mapper，以便服务端/客户端统一使用。

### 2) 服务端：返回 FlatBuffers 对象

```cpp
// 构建 Monster 的 FlatBuffers buffer
auto buffer = std::make_shared<std::vector<uint8_t>>(builder.GetBufferPointer(),
                                                     builder.GetBufferPointer() + builder.GetSize());
// 包装为 Object<Monster>
auto monsterObj = ofb::Object<MyGame::Example::Monster>::fromBuffer(buffer);
return createDtoResponse(Status::CODE_200, monsterObj);
```

ObjectMapper 会将底层二进制直接写入 HTTP 响应，Content-Type 为 `application/x-flatbuffers`。

### 3) 服务端：接收 FlatBuffers 对象

```cpp
return request->readBodyToDtoAsync<ofb::Object<MyGame::Example::Monster>>(controller->getContentMappers()->getDefaultMapper())
  .callbackTo(&EndpointCoroutine::onMonsterRead);

Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monsterObj) {
  const auto* monster = monsterObj.operator->();
  // 访问字段
  auto name = monster->name();
  ...
  return _return(controller->createResponse(Status::CODE_200, "OK"));
}
```

### 4) 客户端：解析响应为 `Object<T>`

```cpp
return response->readBodyToDtoAsync<ofb::Object<MyGame::Example::Monster>>(client->getObjectMapper())
  .callbackTo(&ClientCoroutine::onGetMonsterObj);
```

### 5) 可变访问（可选）

如果需要调用 `mutate_*` 方法，可基于可变缓冲构造：

```cpp
auto mutableBuf = std::make_shared<std::vector<uint8_t>>(...);
auto obj = ofb::Object<MyGame::Example::Monster>::fromMutableBuffer(mutableBuf);
auto* m = obj.getMutable();
if (m) m->mutate_hp(100);
```

注意：FlatBuffers 的可变操作需要在合适的构建选项/布局下才可用，且并非所有字段都支持。请参考 schema 与生成代码的约束。

## 内容类型与协商

- Mapper 信息注册为 `application/x-flatbuffers`
- 使用 `ContentMappers` 时，可直接设置为默认 mapper，或通过 `Accept`/`Content-Type` 进行协商

## API 概览

- `oatpp::flatbuffers::ObjectMapper` 实现 `write`/`read`，直接读写字节流
- `oatpp::flatbuffers::Object<T>` 内部持有 `FlatBuffersWrapper<T>`，保障底层缓冲生命周期，并暴露 `T*`/`const T*`
- 原始字节兼容：也可以直接传 `std::shared_ptr<std::vector<uint8_t>>` 进行写出

## 示例

仓库中包含最小的异步服务端/客户端示例，演示通过 `monster_test.fbs` 定义的 `Monster` 在双方之间传输。

编译产物位于 `build/bin`（例如 `oatpp_flatbuffers_server.exe`、`oatpp_flatbuffers_client.exe`）。

## 生命周期与第三方集成规范

本节供业务方、SDK 或跨模块调用时约定「谁能活多久、该保存什么」。与 `test/object_mapper_read_test.cc` 中的用例一致。

### 核心原则

| 原则 | 说明 |
|------|------|
| **长期持有请用 `Object<T>`** | `ofb::Object<T>`（内部 `FlatBuffersWrapper<T>`）负责延长底层字节缓冲寿命。 |
| **不要单独缓存表指针** | `obj.operator->()`、`monster->name()`、`name()->c_str()` 等均指向**同一块**底层缓冲；`Object` 析构后全部失效。 |
| **需要长期自有数据请 `UnPack()`** | `std::unique_ptr<MonsterT>(obj->UnPack())` 得到堆上原生对象（`std::string` 等已拷贝），可与 `Object` 脱钩使用。 |
| **写出前缓冲必须有效** | `createDtoResponse` 会**同步** `writeToString` 拷贝一份再发 HTTP，不依赖 handler 返回后局部变量仍存活。 |

### HTTP 读入（`readBodyToDto` / `readBodyToDtoAsync`）

典型链路：`BodyDecoder` → `oatpp::String` body → `ObjectMapper::readFromString` → `Object<T>`。

- 当 `Caret` 带有 `getDataMemoryHandle()`（由 `oatpp::String` 构造）时，mapper **不再整包拷贝**到 `vector`，而是把该句柄移入 `FlatBuffersWrapper`（零拷贝借用）。
- 此时 **`Object<T>` 必须一直保留到不再访问任何字段**；销毁 `Object` 即释放 body 对应的 `std::string` 存储。
- 协程回调里若要把数据带到后续逻辑，请 **按值保存 `ofb::Object<T>` 成员**（或先 `UnPack()`），不要只保存 `const T*`。

```cpp
// 推荐：成员持有包装对象
ofb::Object<MyGame::Example::Monster> m_monster;

Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monsterObj) {
  m_monster = monsterObj;  // 共享底层缓冲的引用计数
  return _return(...);
}

// 不推荐：仅缓存裸指针（Object 析构后必悬垂）
const MyGame::Example::Monster* m_bad = nullptr;
Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monsterObj) {
  m_bad = monsterObj.operator->();  // 危险
  return _return(...);
}
```

### 本地构造（`FlatBufferBuilder` / `fromBuffer`）

- **`FlatBufferBuilder` 析构后**，其 `GetBufferPointer()` **立即失效**。必须先拷贝到 `std::vector` 再 `fromBuffer`：

```cpp
flatbuffers::FlatBufferBuilder fbb;
// ... Finish ...
auto buffer = std::make_shared<std::vector<uint8_t>>(
    fbb.GetBufferPointer(),
    fbb.GetBufferPointer() + fbb.GetSize());
auto obj = ofb::Object<MyGame::Example::Monster>::fromBuffer(buffer);
```

- `fromBuffer` 使用 **owned** 路径：包装器持有 `shared_ptr<const vector<uint8_t>>`，与 HTTP 读入的「借用 body 句柄」不同，但同样要求 **`Object` 或 `buffer` 的 `shared_ptr` 至少保留一方** 直到不再读字段。

### 裸指针 `Caret` 与其它非 HTTP 来源

- 若 `Caret(const char*, size)` **没有**内存句柄，mapper 会 **整段拷贝** 到 `vector`（安全默认）。
- 不要假设所有 `read` 都是零拷贝；第三方若自行喂裸指针，应自行保证指针在 `Object` 存活期内有效，或改用 `fromBuffer`/`oatpp::String`。

### 可变缓冲

- 仅 `fromMutableBuffer` + `getMutable()` 路径可变更根表；HTTP 读入的借用路径为**只读**视图。
- 可变字段能否 `mutate_*` 取决于 schema 与 FlatBuffers 生成选项。

### 异步与跨线程

- 在 `readBodyToDtoAsync` 完成后的任意时刻访问字段，只要 **`Object<T>` 仍存活** 即可（包括延迟、排队、下一帧）。
- 跨线程：与其它 `shared_ptr` 一样，需自行保证读写同步；库不额外加锁。

### 校验与其它

- 默认**不**按 schema 自动 `Verifier::Verify`；生产环境建议在业务层对不可信输入校验。
- `readBodyToDtoAsync<Object<T>>` 的 `T` 须在**编译期**固定；运行时多类型请为每种 `T` 注册并分支处理。

### 自测

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++ \
  -DFlatBuffers_DIR=/usr/lib/x86_64-linux-gnu/cmake/flatbuffers \
  -DOATPP_MODULES_LOCATION=EXTERNAL -DOATPP_BUILD_TESTS=ON
cmake --build build -j4
./build/bin/oatpp_flatbuffers_object_mapper_read_test
# 或: ctest --test-dir build -R oatpp_flatbuffers_object_mapper_read_test
```

## 许可证

Apache-2.0
