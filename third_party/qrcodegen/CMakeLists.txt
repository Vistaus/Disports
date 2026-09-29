# Nayuki's QR Code generator (MIT), used to render the QR login code.

add_library(qrcodegen STATIC upstream/cpp/qrcodegen.cpp)
target_include_directories(qrcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/upstream/cpp)
target_compile_features(qrcodegen PUBLIC cxx_std_17)
set_target_properties(qrcodegen PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF)
