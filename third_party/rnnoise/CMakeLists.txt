# Xiph's RNNoise (BSD), neural noise suppression for the microphone in calls.
# Version 0.1.1: its model is small enough for phones (0.2's models cost
# 5-10 times more and don't keep up on little cores).

set(RNNOISE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
add_library(rnnoise STATIC
    ${RNNOISE_DIR}/src/denoise.c
    ${RNNOISE_DIR}/src/rnn.c
    ${RNNOISE_DIR}/src/rnn_data.c
    ${RNNOISE_DIR}/src/rnn_reader.c
    ${RNNOISE_DIR}/src/pitch.c
    ${RNNOISE_DIR}/src/kiss_fft.c
    ${RNNOISE_DIR}/src/celt_lpc.c
)
target_include_directories(rnnoise PUBLIC ${RNNOISE_DIR}/include)
target_link_libraries(rnnoise PRIVATE m)
target_compile_options(rnnoise PRIVATE -w)
set_target_properties(rnnoise PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
