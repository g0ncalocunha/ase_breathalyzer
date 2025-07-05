idf_component_register(SRCS "sd_test.c"
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sd_card
                       WHOLE_ARCHIVE)
