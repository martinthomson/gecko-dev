/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"

#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/jsep/JsepMediaStreamTrack.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepSessionImpl.h"
#include "signaling/src/jsep/JsepTrack.h"

using mozilla::jsep::JsepSessionImpl;
using mozilla::jsep::JsepOfferOptions;
using mozilla::SipccSdpParser;

namespace test {
class JsepSessionTest : public ::testing::Test {
  public:
    JsepSessionTest() {}

    JsepSessionImpl mSession;
    SipccSdpParser mParser;
};

TEST_F(JsepSessionTest, CreateDestroy) {
}

TEST_F(JsepSessionTest, CreateOfferAudio1) {
  JsepOfferOptions options;
  std::string offer;

  nsresult rv = mSession.CreateOffer(options, &offer);
  ASSERT_EQ(NS_OK, rv);

  std::cerr << offer << std::endl;
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

