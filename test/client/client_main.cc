#include "oatpp-flatbuffers/ObjectMapper.hpp"
#include "oatpp-flatbuffers/FlatBuffersWrapper.hpp"
#include "oatpp/web/client/ApiClient.hpp"
#include "oatpp/web/client/HttpRequestExecutor.hpp"
#include "oatpp/web/protocol/http/outgoing/BufferBody.hpp"
#include "oatpp/network/tcp/client/ConnectionProvider.hpp"
#include "oatpp/async/Executor.hpp"
#include "oatpp/macro/codegen.hpp"
#include "monster_test_generated.h"
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

namespace ofb = oatpp::flatbuffers;

class MonsterClient : public oatpp::web::client::ApiClient {
public:
#include OATPP_CODEGEN_BEGIN(ApiClient)
  
  API_CLIENT_INIT(MonsterClient)
  
  // GET /monster - 获取 Monster 对象
  API_CALL_ASYNC("GET", "/monster", GetMonster);

#include OATPP_CODEGEN_END(ApiClient)
  
  // 手动实现 POST /monster，因为需要发送二进制数据
  oatpp::async::CoroutineStarterForResult<const std::shared_ptr<oatpp::web::protocol::http::incoming::Response>&>
  PostMonster(const std::shared_ptr<std::vector<uint8_t>>& buffer) {
    // 将 vector<uint8_t> 转换为 oatpp::String
    oatpp::String bodyString(reinterpret_cast<const char*>(buffer->data()), buffer->size());
    
    // 创建请求体
    auto body = oatpp::web::protocol::http::outgoing::BufferBody::createShared(
        bodyString, 
        "application/x-flatbuffers");
    
    // 创建请求头
    oatpp::web::protocol::http::Headers headers;
    headers.put("Content-Type", "application/x-flatbuffers");
    
    // 解析路径模板
    auto pathTemplate = parsePathTemplate("POST", "/monster");
    
    // 执行请求
    return executeRequestAsync(
        "POST",
        pathTemplate,
        headers,
        {}, // pathParams
        {}, // queryParams
        body);
  }
};

class WaitCoroutine : public oatpp::async::Coroutine<WaitCoroutine> {
  
public:
  WaitCoroutine() {}
  
  Action act() override {
    return waitRepeat(std::chrono::duration<v_int64, std::micro>(1000000));
  }
  
};
class ClientCoroutine : public oatpp::async::Coroutine<ClientCoroutine> {
private:
  std::shared_ptr<MonsterClient> m_client;
  std::shared_ptr<std::vector<uint8_t>> m_monsterBuffer;
  
public:
  ClientCoroutine(const std::shared_ptr<MonsterClient>& client)
      : m_client(client) {}
  
  Action act() override {
    // 首先调用 GET /monster 获取 Monster
    return m_client->GetMonster()
        .callbackTo(&ClientCoroutine::onGetResponse);
  }
  
  Action onGetResponse(
      const std::shared_ptr<oatpp::web::protocol::http::incoming::Response>&
          response) {
    if (response->getStatusCode() != 200) {
      std::cerr << "GET /monster failed with status: "
                << response->getStatusCode() << std::endl;
      return finish();
    }
    
    // 从响应中读取 FlatBuffers 二进制数据
    // 使用readBodyToStringAsync，然后手动转换为buffer
    return response->readBodyToStringAsync()
        .callbackTo(&ClientCoroutine::onGetBodyString);
  }
  
  Action onGetBodyString(const oatpp::String& bodyString) {
    if (!bodyString || bodyString->empty()) {
      std::cerr << "Failed to read Monster buffer from response (empty)" << std::endl;
      return finish();
    }
    
    // 将String转换为vector<uint8_t>
    auto buffer = std::make_shared<std::vector<uint8_t>>(
        reinterpret_cast<const uint8_t*>(bodyString->data()),
        reinterpret_cast<const uint8_t*>(bodyString->data()) + bodyString->size());
    
    return onGetBody(buffer);
  }
  
  Action onGetBody(const std::shared_ptr<std::vector<uint8_t>>& buffer) {
    if (!buffer || buffer->empty()) {
      std::cerr << "Failed to read Monster buffer from response" << std::endl;
      return finish();
    }
    
    std::cout << "Received Monster buffer from GET /monster, size: " 
              << buffer->size() << " bytes" << std::endl;
    
    // 从 buffer 中获取 Monster Table
    void* data = buffer->data();
    const MyGame::Example::Monster* monster = 
        MyGame::Example::GetMonster(data);
    auto monsterT = monster->UnPack();
    
    if (monster) {
      auto name = monster->name();
      auto hp = monster->hp();
      auto mana = monster->mana();
      auto pos = monster->pos();
      auto inventory = monster->inventory();
      std::cout << "Monster - Name: " 
                << (name ? name->c_str() : "null") 
                << ", HP: " << hp 
                << ", Mana: " << mana << std::endl;
      if (pos) {
        std::cout << "Position - X: " << pos->x() 
                  << ", Y: " << pos->y() 
                  << ", Z: " << pos->z() 
                  << std::endl;
      } else {
        std::cout << "Position: null" << std::endl;
      }
      
      // 输出 inventory 信息
      if (inventory) {
        std::cout << "Inventory (" << inventory->size() << " items): ";
        for (size_t i = 0; i < inventory->size(); ++i) {
          std::cout << static_cast<int>(inventory->Get(i));
          if (i < inventory->size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
      } else {
        std::cout << "Inventory: null" << std::endl;
      }

      monsterT->mana = 78;
      monsterT->hp = 33;
      monsterT->name = "tab-game monster";
      monsterT->inventory.push_back(1);
      monsterT->inventory.push_back(2);
      monsterT->inventory.push_back(3);
      monsterT->color = MyGame::Example::Color::Color_Red;
      auto new_pos = std::make_unique<MyGame::Example::Vec3>();
      new_pos->mutate_x(4.0f);
      new_pos->mutate_y(5.0f);
      new_pos->mutate_z(6.0f);
      monsterT->pos = std::move(new_pos);
    }
    ::flatbuffers::FlatBufferBuilder fbb;
    auto monsterOffset = MyGame::Example::CreateMonster(fbb, monsterT);
    fbb.Finish(monsterOffset);
    // 保存 Monster buffer，用于后续的 POST
    m_monsterBuffer = std::make_shared<std::vector<uint8_t>>(
        reinterpret_cast<const uint8_t*>(fbb.GetBufferPointer()),
        reinterpret_cast<const uint8_t*>(fbb.GetBufferPointer() + fbb.GetSize()));
    
    // 现在调用 POST /monster 发送 Monster
    // PostMonster 返回的是 CoroutineStarterForResult
    return m_client->PostMonster(m_monsterBuffer)
        .callbackTo(&ClientCoroutine::onPostResponse);
  }
  
  Action onPostResponse(
      const std::shared_ptr<oatpp::web::protocol::http::incoming::Response>&
          response) {
    if (response->getStatusCode() != 200) {
      std::cerr << "POST /monster failed with status: "
                << response->getStatusCode() << std::endl;
      return finish();
    }
    
    std::cout << "POST /monster succeeded!" << std::endl;
    return finish();
  }
  
  Action handleError(oatpp::async::Error* error) override {
    std::cerr << "Error: " << error->what() << std::endl;
    return error;
  }
};

void runClient() {
  // 初始化 oatpp
  oatpp::Environment::init();
  
  // 创建 FlatBuffers 二进制 ObjectMapper
  auto flatbuffersMapper = std::make_shared<ofb::ObjectMapper>();
  
  // 创建连接提供者
  auto connectionProvider =
      oatpp::network::tcp::client::ConnectionProvider::createShared(
          {"localhost", 8000});
  
  // 创建重试策略
  auto retryPolicy =
      std::make_shared<oatpp::web::client::SimpleRetryPolicy>(5,
                                                              std::chrono::seconds(1));
  
  // 创建请求执行器
  auto requestExecutor =
      oatpp::web::client::HttpRequestExecutor::createShared(connectionProvider,
                                                             retryPolicy);
  
  // 创建客户端
  auto client = MonsterClient::createShared(requestExecutor, flatbuffersMapper);
  
  // 创建异步执行器
  auto executor = std::make_shared<oatpp::async::Executor>(
      4 /* threads */, 1 /* additional threads */,
      1 /* max tasks per thread */);
  
  // 执行客户端协程
  executor->execute<ClientCoroutine>(client);
  executor->execute<WaitCoroutine>();
  
  // 等待执行完成
  executor->waitTasksFinished();
  
  // 停止执行器
  executor->stop();
  executor->join();
  
  // 清理
  oatpp::Environment::destroy();
}

int main() {
  runClient();
  return 0;
}

