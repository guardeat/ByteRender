bytee_add_sample(03_window SOURCES main.cpp LIBRARIES bytee_sample_common)
bytee_add_shaders(03_window SOURCES ${BYTEE_SAMPLES_COMMON_DIR}/shaders/triangle.vert
                                    ${BYTEE_SAMPLES_COMMON_DIR}/shaders/vertex_color.frag)
