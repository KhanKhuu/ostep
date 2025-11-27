set(PACKAGE_NAME counter)

add_library(lib${PACKAGE_NAME} STATIC src/ApproximateCounter.c
                                      src/TraditionalCounter.c)
target_include_directories(lib${PACKAGE_NAME} PUBLIC include)

add_executable(bench_${PACKAGE_NAME} src/bench_counter.c)
target_include_directories(bench_${PACKAGE_NAME} PUBLIC include)
target_link_libraries(bench_${PACKAGE_NAME} PUBLIC lib${PACKAGE_NAME})
