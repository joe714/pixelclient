idf_component_register(
                      SRCS "HttpServer.cc"
                           "StaticAssetHandler.cc"
                           "WebSockets.cc"
              INCLUDE_DIRS "include"
                  REQUIRES "esp_http_server")

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
