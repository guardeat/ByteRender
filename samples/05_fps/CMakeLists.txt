bytee_add_sample(05_fps SOURCES main.cpp scene_pass.cpp sphere_world.cpp
                        LIBRARIES bytee_sample_imgui bytee_sample_common)
bytee_add_shaders(05_fps SOURCES shaders/sphere.vert shaders/sphere.frag shaders/ground.vert shaders/ground.frag)
