#include "oatpp-flatbuffers/ObjectMapper.hpp"
#include "oatpp-flatbuffers/FlatBuffersWrapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/web/server/AsyncHttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/web/mime/ContentMappers.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/macro/codegen.hpp"
#include "oatpp/Types.hpp"
#include "monster_test_generated.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <memory>

namespace ofb = oatpp::flatbuffers;

static std::string ReadAll(const std::string& path) {
  std::string out;
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << path << std::endl;
    return "";
  }
  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);
  out.resize(size);
  file.read(&out[0], size);
  return out;
}

// 辅助函数：创建一个示例 Monster 对象
// 使用 FlatBuffersBuilder 创建一个简单的 Monster
static std::shared_ptr<std::vector<uint8_t>> createSampleMonsterBuffer() {
  flatbuffers::FlatBufferBuilder builder(1024);
  
  // 创建 Monster
  auto name = builder.CreateString("MyMonster");
  auto color = MyGame::Example::Color_Green;
  auto hp = 80;
  auto mana = 150;
  
  // 创建 Vec3
  auto pos = MyGame::Example::Vec3(1.0f, 2.0f, 3.0f, 3.0, color, MyGame::Example::Test(5, 6));
  
  // 创建 inventory
  std::vector<uint8_t> inv = {0, 1, 2, 3, 4};
  auto inventory = builder.CreateVector(inv);
  
  // 创建 Monster
  MyGame::Example::MonsterBuilder monster_builder(builder);
  monster_builder.add_pos(&pos);
  monster_builder.add_hp(hp);
  monster_builder.add_mana(mana);
  monster_builder.add_name(name);
  monster_builder.add_color(color);
  monster_builder.add_inventory(inventory);
  auto monster = monster_builder.Finish();
  
  builder.Finish(monster);
  
  // 获取 buffer
  auto buffer = std::make_shared<std::vector<uint8_t>>(
      builder.GetBufferPointer(),
      builder.GetBufferPointer() + builder.GetSize());
  
  return buffer;
}

class MonsterController : public oatpp::web::server::api::ApiController {
public:
  MonsterController(
      const std::shared_ptr<oatpp::web::mime::ContentMappers>& contentMappers)
      : oatpp::web::server::api::ApiController(contentMappers) {}
  
  static std::shared_ptr<MonsterController> createShared(
      const std::shared_ptr<oatpp::web::mime::ContentMappers>& contentMappers) {
    return std::make_shared<MonsterController>(contentMappers);
  }

#include OATPP_CODEGEN_BEGIN(ApiController)

  ENDPOINT_ASYNC("GET", "/monster", GetMonster) {
    ENDPOINT_ASYNC_INIT(GetMonster)
    
    Action act() override {
      try {
        // 创建示例 Monster buffer 并包装为 Object<Monster>
        auto buffer = createSampleMonsterBuffer();
        auto monsterObj = ofb::Object<MyGame::Example::Monster>::fromBuffer(buffer);
        return _return(controller->createDtoResponse(Status::CODE_200, monsterObj));
      } catch (const std::exception& e) {
        return _return(controller->createResponse(
            Status::CODE_500, oatpp::String("Error: ") + e.what()));
      }
    }
  };

  ENDPOINT_ASYNC("POST", "/monster", PostMonster) {
    ENDPOINT_ASYNC_INIT(PostMonster)
    
    Action act() override {
      // 直接将请求体映射为 Object<Monster>
      return request->readBodyToDtoAsync<ofb::Object<MyGame::Example::Monster>>(controller->getContentMappers()->getDefaultMapper())
          .callbackTo(&PostMonster::onMonsterRead);
    }
    
    Action onMonsterRead(const ofb::Object<MyGame::Example::Monster>& monster) {
      if (!monster) {
        return _return(controller->createResponse(
            Status::CODE_400, "Invalid FlatBuffers data"));
      }
      auto name = monster->name();
      auto hp = monster->hp();
      auto mana = monster->mana();
      auto pos = monster->pos();
      auto inventory = monster->inventory();
      std::cout << "Received Monster - Name: "
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
      return _return(controller->createResponse(Status::CODE_200, "OK"));
    }
  };

#include OATPP_CODEGEN_END(ApiController)
};

void runServer() {
  // 初始化 oatpp
  oatpp::Environment::init();
  
  // 创建 FlatBuffers 二进制 ObjectMapper
  auto flatbuffersMapper = std::make_shared<ofb::ObjectMapper>();
  
  // 创建 ContentMappers 并设置 FlatBuffers mapper
  auto contentMappers = std::make_shared<oatpp::web::mime::ContentMappers>();
  contentMappers->putMapper(flatbuffersMapper);
  contentMappers->setDefaultMapper(flatbuffersMapper);
  
  // 创建路由器
  auto router = oatpp::web::server::HttpRouter::createShared();
  
  // 创建控制器
  auto controller = MonsterController::createShared(contentMappers);
  router->addController(controller);
  
  // 创建连接提供者
  auto connectionProvider = 
      oatpp::network::tcp::server::ConnectionProvider::createShared(
          {"localhost", 8000});
  
  // 创建异步执行器
  auto executor = std::make_shared<oatpp::async::Executor>(
      4 /* threads */, 1 /* additional threads */, 1 /* max tasks per thread */);
  
  // 创建连接处理器
  auto connectionHandler =
      oatpp::web::server::AsyncHttpConnectionHandler::createShared(router, executor);
  
  // 创建服务器
  oatpp::network::Server server(connectionProvider, connectionHandler);
  
  std::cout << "Server running on http://localhost:8000" << std::endl;
  std::cout << "Press CTRL+C to stop..." << std::endl;
  
  // 运行服务器
  server.run();
  
  // 清理
  oatpp::Environment::destroy();
}

int main() {
  runServer();
  return 0;
}


