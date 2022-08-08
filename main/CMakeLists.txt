idf_component_register(SRCS "main.cc"
			    "Animation.cc"
			    "NetManager.cc"
                            "Config.cc"
                            "NvsFlash.cc"
                            "RootPage.cc"
                            "WebPImage.cc"
                       EMBED_FILES nyan_64x32.webp
                       INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
