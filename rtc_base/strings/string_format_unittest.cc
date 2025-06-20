/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/strings/string_format.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/string_encode.h"
#include "test/gtest.h"

namespace webrtc {

TEST(StringFormatTest, Empty) {
  EXPECT_EQ("", StringFormat("%s", ""));
}

TEST(StringFormatTest, Misc) {
  EXPECT_EQ("123hello w", StringFormat("%3d%2s %1c", 123, "hello", 'w'));
  EXPECT_EQ("3 = three", StringFormat("%d = %s", 1 + 2, "three"));
}

TEST(StringFormatTest, MaxSizeShouldWork) {
  const int kSrcLen = 512;
  char str[kSrcLen];
  std::fill_n(str, kSrcLen, 'A');
  str[kSrcLen - 1] = 0;
  EXPECT_EQ(str, StringFormat("%s", str));
}

// Test that formating a string using `absl::string_view` works as expected
// whe using `%.*s`.
TEST(StringFormatTest, FormatStringView) {
  const std::string main_string("This is a substring test.");
  std::vector<absl::string_view> string_views = split(main_string, ' ');
  ASSERT_EQ(string_views.size(), 5u);

  const absl::string_view& sv = string_views[3];
  std::string formatted =
      StringFormat("We have a %.*s.", static_cast<int>(sv.size()), sv.data());
  EXPECT_EQ(formatted.compare("We have a substring."), 0);
}

}  // namespace webrtc
