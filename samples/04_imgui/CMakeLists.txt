bytee_add_sample(04_imgui SOURCES main.cpp LIBRARIES bytee_sample_imgui bytee_sample_common)
bytee_add_shaders(04_imgui SOURCES ${BYTEE_SAMPLES_COMMON_DIR}/shaders/triangle.vert
                                   ${BYTEE_SAMPLES_COMMON_DIR}/shaders/vertex_color.frag)
