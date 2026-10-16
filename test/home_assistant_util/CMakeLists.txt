add_executable(HomeAssistantUtilTest
  HomeAssistantUtilTest.cpp
)

target_include_directories(HomeAssistantUtilTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(HomeAssistantUtilTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(HomeAssistantUtilTest)
