/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/event.h"

#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/clock.h"
#include "gtest/gtest.h"

namespace webrtc {

TEST(EventTest, InitiallySignaled) {
  Event event(false, true);
  ASSERT_TRUE(event.Wait(TimeDelta::Zero()));
}

TEST(EventTest, ManualReset) {
  Event event(true, false);
  ASSERT_FALSE(event.Wait(TimeDelta::Zero()));

  event.Set();
  ASSERT_TRUE(event.Wait(TimeDelta::Zero()));
  ASSERT_TRUE(event.Wait(TimeDelta::Zero()));

  event.Reset();
  ASSERT_FALSE(event.Wait(TimeDelta::Zero()));
}

TEST(EventTest, AutoReset) {
  Event event;
  ASSERT_FALSE(event.Wait(TimeDelta::Zero()));

  event.Set();
  ASSERT_TRUE(event.Wait(TimeDelta::Zero()));
  ASSERT_FALSE(event.Wait(TimeDelta::Zero()));
}

class SignalerThread {
 public:
  void Start(Event* writer, Event* reader) {
    writer_ = writer;
    reader_ = reader;
    thread_ = PlatformThread::SpawnJoinable(
        [this] {
          while (!stop_event_.Wait(TimeDelta::Zero())) {
            writer_->Set();
            reader_->Wait(Event::kForever);
          }
        },
        "EventPerf");
  }
  void Stop() {
    stop_event_.Set();
    thread_.Finalize();
  }
  Event stop_event_;
  Event* writer_;
  Event* reader_;
  PlatformThread thread_;
};

TEST(EventTest, UnsignaledWaitDoesNotReturnBeforeTimeout) {
  constexpr TimeDelta kDuration = TimeDelta::Micros(10'499);
  Event event;
  auto begin = Clock::GetRealTimeClock()->CurrentTime();
  EXPECT_FALSE(event.Wait(kDuration));
  EXPECT_GE(Clock::GetRealTimeClock()->CurrentTime(), begin + kDuration);
}

// These tests are disabled by default and only intended to be run manually.
TEST(EventTest, DISABLED_PerformanceSingleThread) {
  static const int kNumIterations = 10000000;
  Event event;
  for (int i = 0; i < kNumIterations; ++i) {
    event.Set();
    event.Wait(TimeDelta::Zero());
  }
}

TEST(EventTest, DISABLED_PerformanceMultiThread) {
  static const int kNumIterations = 10000;
  Event read;
  Event write;
  SignalerThread thread;
  thread.Start(&read, &write);

  for (int i = 0; i < kNumIterations; ++i) {
    write.Set();
    read.Wait(Event::kForever);
  }
  write.Set();

  thread.Stop();
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Tests that we crash if we attempt to call Event::Wait while we're
// not allowed to (as per `RTC_DISALLOW_WAIT()`).
TEST(EventTestDeathTest, DisallowEventWait) {
  Event event;
  RTC_DISALLOW_WAIT();
  EXPECT_DEATH(event.Wait(Event::kForever), "");
}
#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
