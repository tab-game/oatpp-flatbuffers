cmake_minimum_required(VERSION 3.15)

# 通用函数：为示例可执行程序做统一构建配置
function(add_ofb_example TARGET_NAME)
  # args: TARGET_NAME <sources...>
  set(options)
  set(oneValueArgs)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(AOE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  add_executable(${TARGET_NAME}
    ${AOE_SOURCES}
  )

  set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
  )

  target_include_directories(${TARGET_NAME}
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  )

  target_link_oatpp(${TARGET_NAME})
  target_link_libraries(${TARGET_NAME}
    PRIVATE oatpp-flatbuffers
            flatbuffers::flatbuffers
  )

  if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /utf-8)
  endif()

  # 拷贝测试资源到可执行程序同级 res/ 目录
  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/res
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/monster_test.bfbs
            $<TARGET_FILE_DIR:${TARGET_NAME}>/res/monster_test.bfbs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/monster_test.fbs
            $<TARGET_FILE_DIR:${TARGET_NAME}>/res/monster_test.fbs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/monsterdata_test.json
            $<TARGET_FILE_DIR:${TARGET_NAME}>/res/monsterdata_test.json
  )
endfunction()

# Server 可执行程序
add_ofb_example(oatpp_flatbuffers_server
  SOURCES
    server/server_main.cc
)

# Client 可执行程序
add_ofb_example(oatpp_flatbuffers_client
  SOURCES
    client/client_main.cc
)

# Demo 可执行程序（如果存在）
if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/demo_main.cc)
  add_ofb_example(oatpp_flatbuffers_demo
    SOURCES
      demo_main.cc
  )
endif()

