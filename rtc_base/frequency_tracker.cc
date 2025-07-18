/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/frequency_tracker.h"

#include <cstdint>
#include <optional>

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/rate_statistics.h"

namespace webrtc {

FrequencyTracker::FrequencyTracker(TimeDelta max_window_size)
    : impl_(max_window_size.ms(), 1'000'000) {}

std::optional<Frequency> FrequencyTracker::Rate(Timestamp now) const {
  if (std::optional<int64_t> rate = impl_.Rate(now.ms())) {
    return Frequency::MilliHertz(*rate);
  }
  return std::nullopt;
}

void FrequencyTracker::Update(int64_t count, Timestamp now) {
  impl_.Update(count, now.ms());
}

}  // namespace webrtc
