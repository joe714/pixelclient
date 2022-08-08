idf_component_register(
          SRCS "Hub75Display.c"
               "i2s_parallel.c"
               "val2pwm.c"
          INCLUDE_DIRS "include"
          REQUIRES "driver"
    )
