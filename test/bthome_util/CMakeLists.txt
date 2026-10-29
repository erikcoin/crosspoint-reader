add_executable(BthomeUtilTest
  BthomeUtilTest.cpp
)

target_include_directories(BthomeUtilTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(BthomeUtilTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BthomeUtilTest)
