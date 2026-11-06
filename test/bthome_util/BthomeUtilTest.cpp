#include <gtest/gtest.h>

#include <cstring>

#include "BthomeUtil.h"

TEST(Bthome, BuildsPayloadForSecondOfFourButtons) {
  uint8_t buf[bthome::MAX_PAYLOAD];
  const size_t n = bthome::buildButtonPayload(7, 4, 1, buf, sizeof(buf));
  const uint8_t expected[] = {0x44, 0x00, 0x07, 0x3A, 0x00, 0x3A, 0x01, 0x3A, 0x00, 0x3A, 0x00};
  ASSERT_EQ(n, sizeof(expected));
  EXPECT_EQ(0, std::memcmp(buf, expected, n));
}

TEST(Bthome, SizeFollowsButtonCount) {
  uint8_t buf[bthome::MAX_PAYLOAD];
  EXPECT_EQ(5u, bthome::buildButtonPayload(1, 1, 0, buf, sizeof(buf)));
  EXPECT_EQ(7u, bthome::buildButtonPayload(1, 2, 1, buf, sizeof(buf)));
}

TEST(Bthome, RejectsBadArguments) {
  uint8_t buf[bthome::MAX_PAYLOAD];
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, 0, 0, buf, sizeof(buf)));
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, bthome::MAX_BUTTONS + 1, 0, buf, sizeof(buf)));
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, 4, 4, buf, sizeof(buf)));
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, 4, 0, buf, 10));
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, 4, 0, nullptr, 20));
}

TEST(Bthome, LongPressEventReplacesPress) {
  uint8_t buf[bthome::MAX_PAYLOAD];
  const size_t n = bthome::buildButtonPayload(3, 2, 0, buf, sizeof(buf), bthome::EVENT_LONG_PRESS);
  const uint8_t expected[] = {0x44, 0x00, 0x03, 0x3A, 0x04, 0x3A, 0x00};
  ASSERT_EQ(n, sizeof(expected));
  EXPECT_EQ(0, std::memcmp(buf, expected, n));
}

TEST(Bthome, RejectsUnknownEventValues) {
  uint8_t buf[bthome::MAX_PAYLOAD];
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, 2, 0, buf, sizeof(buf), 0));
  EXPECT_EQ(0u, bthome::buildButtonPayload(1, 2, 0, buf, sizeof(buf), 7));
}

TEST(Bthome, EventNamesMapToValues) {
  EXPECT_EQ(bthome::EVENT_PRESS, bthome::eventFromName("press"));
  EXPECT_EQ(bthome::EVENT_DOUBLE_PRESS, bthome::eventFromName("double_press"));
  EXPECT_EQ(bthome::EVENT_LONG_PRESS, bthome::eventFromName("long_press"));
  EXPECT_EQ(bthome::EVENT_LONG_TRIPLE_PRESS, bthome::eventFromName("long_triple_press"));
  EXPECT_EQ(bthome::EVENT_NONE, bthome::eventFromName("hold"));
  EXPECT_EQ(bthome::EVENT_NONE, bthome::eventFromName(nullptr));
}
