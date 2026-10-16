#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "HomeAssistantUtil.h"

namespace {

std::string parse(const std::string& body, const size_t cap = 16) {
  char out[64] = "untouched";
  if (!haUtil::parseState(body.c_str(), body.size(), out, cap)) return "<none>";
  return out;
}

TEST(HomeAssistantUtil, EntityIdMustBeDomainDotObject) {
  EXPECT_TRUE(haUtil::isValidEntityId("light.bibliotheeklamp"));
  EXPECT_TRUE(haUtil::isValidEntityId("media_player.jbl_bar_9_1_3818"));
  EXPECT_FALSE(haUtil::isValidEntityId(""));
  EXPECT_FALSE(haUtil::isValidEntityId("light"));
  EXPECT_FALSE(haUtil::isValidEntityId(".lamp"));
  EXPECT_FALSE(haUtil::isValidEntityId("light."));
  EXPECT_FALSE(haUtil::isValidEntityId("light.a.b"));
  EXPECT_FALSE(haUtil::isValidEntityId("Light.Lamp"));
  EXPECT_FALSE(haUtil::isValidEntityId("light.lamp/../x"));
  EXPECT_FALSE(haUtil::isValidEntityId("light.lamp x"));
}

TEST(HomeAssistantUtil, PathsUseTheEntitysDomain) {
  EXPECT_EQ(haUtil::statePath("light.keukentafellamp"), "/api/states/light.keukentafellamp");
  EXPECT_EQ(haUtil::toggleServicePath("light.keukentafellamp"), "/api/services/light/toggle");
  EXPECT_EQ(haUtil::toggleServicePath("media_player.jbl_bar_9_1_3818"), "/api/services/media_player/toggle");
  EXPECT_EQ(haUtil::toggleBody("light.staandelampen"), "{\"entity_id\":\"light.staandelampen\"}");
}

TEST(HomeAssistantUtil, SplitsBaseUrl) {
  haUtil::UrlParts p;
  ASSERT_TRUE(haUtil::splitBaseUrl("http://homeassistant.local:8123", p));
  EXPECT_EQ(p.scheme, "http");
  EXPECT_EQ(p.host, "homeassistant.local");
  EXPECT_EQ(p.port, "8123");
  EXPECT_EQ(haUtil::joinBaseUrl(p), "http://homeassistant.local:8123");

  ASSERT_TRUE(haUtil::splitBaseUrl("https://ha.example.org/", p));
  EXPECT_EQ(p.host, "ha.example.org");
  EXPECT_EQ(p.port, "");
  EXPECT_EQ(haUtil::joinBaseUrl(p), "https://ha.example.org");

  p.host = "192.168.1.20";
  p.port = "8123";
  EXPECT_EQ(haUtil::joinBaseUrl(p), "https://192.168.1.20:8123");
}

TEST(HomeAssistantUtil, RejectsUnusableBaseUrls) {
  haUtil::UrlParts p;
  EXPECT_FALSE(haUtil::splitBaseUrl("", p));
  EXPECT_FALSE(haUtil::splitBaseUrl("homeassistant.local:8123", p));
  EXPECT_FALSE(haUtil::splitBaseUrl("ftp://homeassistant.local", p));
  EXPECT_FALSE(haUtil::splitBaseUrl("http://", p));
  EXPECT_FALSE(haUtil::splitBaseUrl("http://host:abc", p));
  EXPECT_FALSE(haUtil::splitBaseUrl("http://host:", p));
  EXPECT_FALSE(haUtil::splitBaseUrl("http://host/some/path", p));
}

TEST(HomeAssistantUtil, DetectsMdnsHosts) {
  EXPECT_TRUE(haUtil::isMdnsHost("homeassistant.local"));
  EXPECT_TRUE(haUtil::isMdnsHost("HomeAssistant.LOCAL"));
  EXPECT_FALSE(haUtil::isMdnsHost("192.168.1.20"));
  EXPECT_FALSE(haUtil::isMdnsHost("ha.example.org"));
  EXPECT_FALSE(haUtil::isMdnsHost(".local"));
  EXPECT_FALSE(haUtil::isMdnsHost("local"));
}

TEST(HomeAssistantUtil, ParsesStateFromSingleEntityResponse) {
  EXPECT_EQ(parse(R"({"entity_id":"light.a","state":"on","attributes":{"brightness":120}})"), "On");
  EXPECT_EQ(parse(R"({"entity_id": "light.a", "state": "off", "attributes": {}})"), "Off");
}

TEST(HomeAssistantUtil, ParsesStateFromServiceResponseList) {
  EXPECT_EQ(parse(R"([{"entity_id":"media_player.x","state":"playing","attributes":{}}])"), "Playing");
  EXPECT_EQ(parse("[]"), "<none>");
}

TEST(HomeAssistantUtil, IgnoresAStateValueThatWasCutOff) {
  EXPECT_EQ(parse(R"({"entity_id":"light.a","state":"unavail)"), "<none>");
  EXPECT_EQ(parse(R"({"entity_id":"light.a","state":)"), "<none>");
  EXPECT_EQ(parse(R"({"entity_id":"light.a","state":"")"), "<none>");
}

TEST(HomeAssistantUtil, LongValuesAreTruncatedToTheBuffer) {
  EXPECT_EQ(parse(R"({"state":"abcdefghijklmnopqrstuvwxyz"})", 8), "Abcdefg");
}

TEST(HomeAssistantUtil, RejectsBadArguments) {
  char out[8];
  EXPECT_FALSE(haUtil::parseState(nullptr, 0, out, sizeof(out)));
  EXPECT_FALSE(haUtil::parseState("{\"state\":\"on\"}", 14, nullptr, 8));
  EXPECT_FALSE(haUtil::parseState("{\"state\":\"on\"}", 14, out, 1));
}

}  // namespace
