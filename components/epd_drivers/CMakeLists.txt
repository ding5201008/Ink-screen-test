# 墨水屏驱动组件
idf_component_register(SRCS "src/epd_common.c"
                             "src/epd_ssd1619.c"
                             "src/epd_il3820.c"
                             "src/epd_uc8151.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver spi_flash)
