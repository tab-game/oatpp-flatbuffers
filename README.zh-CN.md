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

## 注意事项

- 校验：默认不自动按 schema 校验，可在需要时自行使用 `flatbuffers::Verifier`
- 类型绑定：`readBodyToDtoAsync<Object<T>>` 需要在编译期确定目标 `T`；内部通过类型注册表创建对应包装
- 内存：包装器会持有底层 `std::vector<uint8_t>`，请注意复制/共享的开销

## 许可证

Apache-2.0
