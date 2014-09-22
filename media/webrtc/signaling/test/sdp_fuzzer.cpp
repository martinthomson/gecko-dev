/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "timecard.h"

#include <string>
#include <sstream>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "mtransport_test_utils.h"
MtransportTestUtils *test_utils;

#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SdpAttribute.h"

/*
  extern "C" {
  #include "signaling/src/sdp/sipcc/sdp.h"
  }
*/

using namespace mozilla;

namespace test {

class SdpTest : public ::testing::Test {
  public:
    SdpTest() : sdp_ptr_(nullptr) {
    }

    static void SetUpTestCase() {
      // TODO get random
    }

    void SetUp() {
      final_level_ = 0;
      sdp_ptr_ = nullptr;
    }

    static void TearDownTestCase() {
    }

 protected:
    SipccSdpParser mParser;
    mozilla::UniquePtr<Sdp> mSdp;
};


int main(int argc, char **argv) {
  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  sipcc::PeerConnectionCtx::Destroy();
  delete test_utils;

  return result;
}
