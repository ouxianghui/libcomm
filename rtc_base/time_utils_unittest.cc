/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/time_utils.h"

#include <cstdint>
#include <ctime>
#include <memory>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {

TEST(TimeTest, TimeInMs) {
  int64_t ts_earlier = TimeMillis();
  Thread::SleepMs(100);
  int64_t ts_now = TimeMillis();
  // Allow for the thread to wakeup ~20ms early.
  EXPECT_GE(ts_now, ts_earlier + 80);
  // Make sure the Time is not returning in smaller unit like microseconds.
  EXPECT_LT(ts_now, ts_earlier + 1000);
}

TEST(TimeTest, Intervals) {
  int64_t ts_earlier = TimeMillis();
  int64_t ts_later = TimeAfter(500);

  // We can't depend on ts_later and ts_earlier to be exactly 500 apart
  // since time elapses between the calls to TimeMillis() and TimeAfter(500)
  EXPECT_LE(500, TimeDiff(ts_later, ts_earlier));
  EXPECT_GE(-500, TimeDiff(ts_earlier, ts_later));

  // Time has elapsed since ts_earlier
  EXPECT_GE(TimeSince(ts_earlier), 0);

  // ts_earlier is earlier than now, so TimeUntil ts_earlier is -ve
  EXPECT_LE(TimeUntil(ts_earlier), 0);

  // ts_later likely hasn't happened yet, so TimeSince could be -ve
  // but within 500
  EXPECT_GE(TimeSince(ts_later), -500);

  // TimeUntil ts_later is at most 500
  EXPECT_LE(TimeUntil(ts_later), 500);
}

TEST(TimeTest, TestTimeDiff64) {
  int64_t ts_diff = 100;
  int64_t ts_earlier = TimeMillis();
  int64_t ts_later = ts_earlier + ts_diff;
  EXPECT_EQ(ts_diff, TimeDiff(ts_later, ts_earlier));
  EXPECT_EQ(-ts_diff, TimeDiff(ts_earlier, ts_later));
}

class TmToSecondsTest : public ::testing::Test {
 public:
  TmToSecondsTest() {
    // Set use of the test RNG to get deterministic expiration timestamp.
    SetRandomTestMode(true);
  }
  ~TmToSecondsTest() override {
    // Put it back for the next test.
    SetRandomTestMode(false);
  }

  void TestTmToSeconds(int times) {
    static char mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 0; i < times; i++) {
      // First generate something correct and check that TmToSeconds is happy.
      int year = CreateRandomId() % 400 + 1970;

      bool leap_year = false;
      if (year % 4 == 0)
        leap_year = true;
      if (year % 100 == 0)
        leap_year = false;
      if (year % 400 == 0)
        leap_year = true;

      std::tm tm;
      tm.tm_year = year - 1900;  // std::tm is year 1900 based.
      tm.tm_mon = CreateRandomId() % 12;
      tm.tm_mday = CreateRandomId() % mdays[tm.tm_mon] + 1;
      tm.tm_hour = CreateRandomId() % 24;
      tm.tm_min = CreateRandomId() % 60;
      tm.tm_sec = CreateRandomId() % 60;
      int64_t t = TmToSeconds(tm);
      EXPECT_TRUE(t >= 0);

      // Now damage a random field and check that TmToSeconds is unhappy.
      switch (CreateRandomId() % 11) {
        case 0:
          tm.tm_year = 1969 - 1900;
          break;
        case 1:
          tm.tm_mon = -1;
          break;
        case 2:
          tm.tm_mon = 12;
          break;
        case 3:
          tm.tm_mday = 0;
          break;
        case 4:
          tm.tm_mday = mdays[tm.tm_mon] + (leap_year && tm.tm_mon == 1) + 1;
          break;
        case 5:
          tm.tm_hour = -1;
          break;
        case 6:
          tm.tm_hour = 24;
          break;
        case 7:
          tm.tm_min = -1;
          break;
        case 8:
          tm.tm_min = 60;
          break;
        case 9:
          tm.tm_sec = -1;
          break;
        case 10:
          tm.tm_sec = 60;
          break;
      }
      EXPECT_EQ(TmToSeconds(tm), -1);
    }
    // Check consistency with the system gmtime_r.  With time_t, we can only
    // portably test dates until 2038, which is achieved by the % 0x80000000.
    for (int i = 0; i < times; i++) {
      time_t t = CreateRandomId() % 0x80000000;
#if defined(WEBRTC_WIN)
      std::tm* tm = std::gmtime(&t);
      EXPECT_TRUE(tm);
      EXPECT_TRUE(TmToSeconds(*tm) == t);
#else
      std::tm tm;
      EXPECT_TRUE(gmtime_r(&t, &tm));
      EXPECT_TRUE(TmToSeconds(tm) == t);
#endif
    }
  }
};

TEST_F(TmToSecondsTest, TestTmToSeconds) {
  TestTmToSeconds(100000);
}

// Test that all the time functions exposed by TimeUtils get time from the
// fake clock when it's set.
TEST(FakeClock, TimeFunctionsUseFakeClock) {
  FakeClock clock;
  SetClockForTesting(&clock);

  clock.SetTime(Timestamp::Micros(987654));
  EXPECT_EQ(987u, Time32());
  EXPECT_EQ(987, TimeMillis());
  EXPECT_EQ(987654, TimeMicros());
  EXPECT_EQ(987654000, TimeNanos());
  EXPECT_EQ(1000u, TimeAfter(13));

  SetClockForTesting(nullptr);
  // After it's unset, we should get a normal time.
  EXPECT_NE(987, TimeMillis());
}

TEST(FakeClock, InitialTime) {
  FakeClock clock;
  EXPECT_EQ(0, clock.TimeNanos());
}

TEST(FakeClock, SetTime) {
  FakeClock clock;
  clock.SetTime(Timestamp::Micros(123));
  EXPECT_EQ(123000, clock.TimeNanos());
  clock.SetTime(Timestamp::Micros(456));
  EXPECT_EQ(456000, clock.TimeNanos());
}

TEST(FakeClock, AdvanceTime) {
  FakeClock clock;
  clock.AdvanceTime(TimeDelta::Micros(1u));
  EXPECT_EQ(1000, clock.TimeNanos());
  clock.AdvanceTime(TimeDelta::Micros(2222u));
  EXPECT_EQ(2223000, clock.TimeNanos());
  clock.AdvanceTime(TimeDelta::Millis(3333u));
  EXPECT_EQ(3335223000, clock.TimeNanos());
  clock.AdvanceTime(TimeDelta::Seconds(4444u));
  EXPECT_EQ(4447335223000, clock.TimeNanos());
}

// When the clock is advanced, threads that are waiting in a socket select
// should wake up and look at the new time. This allows tests using the
// fake clock to run much faster, if the test is bound by time constraints
// (such as a test for a STUN ping timeout).
TEST(FakeClock, SettingTimeWakesThreads) {
  int64_t real_start_time_ms = TimeMillis();

  ThreadProcessingFakeClock clock;
  SetClockForTesting(&clock);

  std::unique_ptr<Thread> worker(Thread::CreateWithSocketServer());
  worker->Start();

  // Post an event that won't be executed for 10 seconds.
  Event message_handler_dispatched;
  worker->PostDelayedTask(
      [&message_handler_dispatched] { message_handler_dispatched.Set(); },
      TimeDelta::Seconds(60));

  // Wait for a bit for the worker thread to be started and enter its socket
  // select(). Otherwise this test would be trivial since the worker thread
  // would process the event as soon as it was started.
  Thread::Current()->SleepMs(1000);

  // Advance the fake clock, expecting the worker thread to wake up
  // and dispatch the message instantly.
  clock.AdvanceTime(TimeDelta::Seconds(60u));
  EXPECT_TRUE(message_handler_dispatched.Wait(TimeDelta::Zero()));
  worker->Stop();

  SetClockForTesting(nullptr);

  // The message should have been dispatched long before the 60 seconds fully
  // elapsed (just a sanity check).
  int64_t real_end_time_ms = TimeMillis();
  EXPECT_LT(real_end_time_ms - real_start_time_ms, 10000);
}

}  // namespace webrtc
