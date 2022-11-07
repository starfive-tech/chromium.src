// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include "ash/components/settings/scoped_timezone_settings.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/date_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"
#include "base/time/time.h"

namespace ash {

namespace {

void SetDefaultLocale(const std::string& lang) {
  base::i18n::SetICUDefaultLocale(lang);
  ash::DateHelper::GetInstance()->ResetForTesting();
}

}  // namespace

using CalendarUtilsUnittest = AshTestBase;

// Tests the time difference calculation with different timezones and
// considering daylight savings.
TEST_F(CalendarUtilsUnittest, GetTimeDifference) {
  // Create a date: Aug,1st 2021.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Sets the timezone to "America/Los_Angeles";
  ash::system::ScopedTimezoneSettings timezone_settings(u"PST");

  // Before daylight saving the time difference is 7 hours.
  EXPECT_EQ(base::Minutes(-420), calendar_utils::GetTimeDifference(date));

  // Create a date after daylight saving: Dec,1st 2021.
  base::Time date2;
  ASSERT_TRUE(base::Time::FromString("1 Dec 2021 10:00 GMT", &date2));

  // After daylight saving the time difference is 8 hours.
  EXPECT_EQ(base::Minutes(-480), calendar_utils::GetTimeDifference(date2));

  // Set the timezone to GMT.
  timezone_settings.SetTimezoneFromID(u"GMT");

  EXPECT_EQ(base::Minutes(0), calendar_utils::GetTimeDifference(date));
  EXPECT_EQ(base::Minutes(0), calendar_utils::GetTimeDifference(date2));
}

TEST_F(CalendarUtilsUnittest, DateFormatter) {
  // Create a date: Aug 1, 2021.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Test DateFormatter to return date in "MMMMdyyyy" format.
  EXPECT_EQ(u"August 1, 2021", calendar_utils::GetMonthDayYear(date));

  // Test DateFormatter to return month name.
  EXPECT_EQ(u"August", calendar_utils::GetMonthName(date));

  // Test DateFormatter to return day of month.
  EXPECT_EQ(u"1", calendar_utils::GetDayOfMonth(date));

  // Test DateFormatter to return month name and day of month.
  EXPECT_EQ(u"August 1", calendar_utils::GetMonthNameAndDayOfMonth(date));

  // Test DateFormatter to return the time zone.
  EXPECT_EQ(u"Greenwich Mean Time", calendar_utils::GetTimeZone(date));

  // Test DateFormatter to return year.
  EXPECT_EQ(u"2021", calendar_utils::GetYear(date));

  // Test DateFormatter to return month name and year.
  EXPECT_EQ(u"August 2021", calendar_utils::GetMonthNameAndYear(date));
}

TEST_F(CalendarUtilsUnittest, DateFormatterClockTimes) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Using "en" locale as other languages format their hours differently.
  SetDefaultLocale("en");

  // Create AM time: 9:05 GMT.
  base::Time am_time;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 9:05 GMT", &am_time));

  // Create PM time: 23:30 GMT.
  base::Time pm_time;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 23:30 GMT", &pm_time));

  // Create midnight: 00:00 GMT.
  base::Time midnight;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 00:00 GMT", &midnight));

  // Return time in twelve hour clock format. (no '0' padding)
  EXPECT_EQ(u"9:05 AM", calendar_utils::GetTwelveHourClockTime(am_time));
  EXPECT_EQ(u"11:30 PM", calendar_utils::GetTwelveHourClockTime(pm_time));
  EXPECT_EQ(u"12:00 AM", calendar_utils::GetTwelveHourClockTime(midnight));

  // Return time in twenty four hour clock format. (has '0' padding)
  EXPECT_EQ(u"09:05", calendar_utils::GetTwentyFourHourClockTime(am_time));
  EXPECT_EQ(u"23:30", calendar_utils::GetTwentyFourHourClockTime(pm_time));
  EXPECT_EQ(u"00:00", calendar_utils::GetTwentyFourHourClockTime(midnight));

  // Return single hours in twelve hour format. (no '0' padding)
  EXPECT_EQ(u"9", calendar_utils::GetTwelveHourClockHours(am_time));
  EXPECT_EQ(u"11", calendar_utils::GetTwelveHourClockHours(pm_time));
  EXPECT_EQ(u"12", calendar_utils::GetTwelveHourClockHours(midnight));

  // Return single hours in twenty four hour format. (has '0' padding)
  EXPECT_EQ(u"09", calendar_utils::GetTwentyFourHourClockHours(am_time));
  EXPECT_EQ(u"23", calendar_utils::GetTwentyFourHourClockHours(pm_time));
  EXPECT_EQ(u"00", calendar_utils::GetTwentyFourHourClockHours(midnight));

  // Return minutes. (has '0' padding)
  EXPECT_EQ(u"05", calendar_utils::GetMinutes(am_time));
  EXPECT_EQ(u"30", calendar_utils::GetMinutes(pm_time));
  EXPECT_EQ(u"00", calendar_utils::GetMinutes(midnight));
}

TEST_F(CalendarUtilsUnittest, HoursAndMinutesInDifferentLocales) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Create AM time: 9:05 GMT.
  base::Time am_time;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 9:05 GMT", &am_time));

  // Create PM time: 23:30 GMT.
  base::Time pm_time;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 23:30 GMT", &pm_time));

  // Create midnight: 00:00 GMT.
  base::Time midnight;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 00:00 GMT", &midnight));

  for (auto* locale : kLocales) {
    // Skip locales that are tested in "LocalesWithUniqueNumerals".
    if (kLocalesWithUniqueNumerals.count(locale))
      continue;

    SetDefaultLocale(locale);

    // If the length of the hour string is more than 1 in a single digit hour
    // (9AM) then it is zero-padded.
    bool zero_padded_12H =
        calendar_utils::GetTwelveHourClockHours(am_time).length() > 1;
    bool zero_padded_24H =
        calendar_utils::GetTwentyFourHourClockHours(am_time).length() > 1;

    // Return hours in twelve hour format.
    EXPECT_EQ(zero_padded_12H ? u"09" : u"9",
              calendar_utils::GetTwelveHourClockHours(am_time));
    EXPECT_EQ(u"11", calendar_utils::GetTwelveHourClockHours(pm_time));
    // Locale 'ja'uses  'K' format (0~11) for its 12-hour clock.
    EXPECT_EQ((strcmp(locale, "ja") == 0) ? u"0" : u"12",
              calendar_utils::GetTwelveHourClockHours(midnight));

    // Return hours in twenty four hour format.
    EXPECT_EQ(zero_padded_24H ? u"09" : u"9",
              calendar_utils::GetTwentyFourHourClockHours(am_time));
    EXPECT_EQ(u"23", calendar_utils::GetTwentyFourHourClockHours(pm_time));
    EXPECT_EQ(zero_padded_24H ? u"00" : u"0",
              calendar_utils::GetTwentyFourHourClockHours(midnight));

    // Return minutes. (always zero-padded)
    EXPECT_EQ(u"05", calendar_utils::GetMinutes(am_time));
    EXPECT_EQ(u"30", calendar_utils::GetMinutes(pm_time));
    EXPECT_EQ(u"00", calendar_utils::GetMinutes(midnight));
  }

  // Reset locale to English for subsequent tests.
  SetDefaultLocale("en");
}

TEST_F(CalendarUtilsUnittest, LocalesWithUniqueNumerals) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Create time: 23:03 GMT.
  base::Time time;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 23:03 GMT", &time));

  for (auto locale : kLocalesWithUniqueNumerals) {
    SetDefaultLocale(locale);

    if (locale == "bn") {
      EXPECT_EQ(u"২৩", calendar_utils::GetTwentyFourHourClockHours(time));
      EXPECT_EQ(u"০৩", calendar_utils::GetMinutes(time));
    } else if (locale == "fa" || locale == "pa-pk") {
      EXPECT_EQ(u"۲۳", calendar_utils::GetTwentyFourHourClockHours(time));
      EXPECT_EQ(u"۰۳", calendar_utils::GetMinutes(time));
    } else if (locale == "mr") {
      EXPECT_EQ(u"२३", calendar_utils::GetTwentyFourHourClockHours(time));
      EXPECT_EQ(u"०३", calendar_utils::GetMinutes(time));
    } else
      EXPECT_TRUE(false) << "Locale '" << locale << "' needs a test case.";
  }

  // Reset locale to English for subsequent tests.
  SetDefaultLocale("en");
}

TEST_F(CalendarUtilsUnittest, IntervalFormatter) {
  base::Time date1;
  base::Time date2;
  base::Time date3;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date1));
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 11:30 GMT", &date2));
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 15:49 GMT", &date3));

  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  EXPECT_EQ(u"10:00 – 11:30 AM",
            calendar_utils::FormatTwelveHourClockTimeInterval(date1, date2));

  EXPECT_EQ(u"10:00 AM – 3:49 PM",
            calendar_utils::FormatTwelveHourClockTimeInterval(date1, date3));

  EXPECT_EQ(
      u"10:00 – 11:30",
      calendar_utils::FormatTwentyFourHourClockTimeInterval(date1, date2));

  EXPECT_EQ(
      u"10:00 – 15:49",
      calendar_utils::FormatTwentyFourHourClockTimeInterval(date1, date3));
}

TEST_F(CalendarUtilsUnittest, TimezoneChanged) {
  // Create a date: Aug,1st 2021.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 3:00 GMT", &date));
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Test DateFormatter to return the time zone.
  EXPECT_EQ(u"Greenwich Mean Time", calendar_utils::GetTimeZone(date));

  // Test DateFormatter to return date in "MMMMdyyyy" format.
  EXPECT_EQ(u"August 1, 2021", calendar_utils::GetMonthDayYear(date));

  // Set timezone to Pacific Daylight Time (date changes to previous day).
  timezone_settings.SetTimezoneFromID(u"PST");

  // Test DateFormatter to return the time zone.
  EXPECT_EQ(u"Pacific Daylight Time", calendar_utils::GetTimeZone(date));

  // Test DateFormatter to return date in "MMMMdyyyy" format.
  EXPECT_EQ(u"July 31, 2021", calendar_utils::GetMonthDayYear(date));
}

TEST_F(CalendarUtilsUnittest, GetMonthsBetween) {
  base::Time start_date, end_date;

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 0);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Nov 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 1);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Sep 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), -1);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Oct 2010 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 12);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Oct 2008 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), -12);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Apr 2010 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 6);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Apr 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), -6);

  ASSERT_TRUE(base::Time::FromString("01 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("02 Oct 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 0);

  ASSERT_TRUE(base::Time::FromString("01 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("31 Oct 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 0);

  ASSERT_TRUE(base::Time::FromString("31 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("01 Nov 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 1);

  ASSERT_TRUE(base::Time::FromString("01 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("30 Sep 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), -1);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Oct 2022 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 13 * 12);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Oct 1996 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), -13 * 12);

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Dec 2022 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date),
            (13 * 12) + 2);
  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("23 Dec 1996 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date),
            (-13 * 12) + 2);
  ASSERT_TRUE(base::Time::FromString("31 Dec 2009 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("01 Jan 2010 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), 1);
  ASSERT_TRUE(base::Time::FromString("01 Jan 2010 11:00 GMT", &start_date));
  ASSERT_TRUE(base::Time::FromString("31 Dec 2009 11:00 GMT", &end_date));
  EXPECT_EQ(calendar_utils::GetMonthsBetween(start_date, end_date), -1);
}

TEST_F(CalendarUtilsUnittest, GetFetchStartEndTimes) {
  base::Time date, expected_start, expected_end;
  std::pair<base::Time, base::Time> fetch;

  // Event and timezone are both GMT, no difference.
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 00:00 GMT", &date));
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 00:00 GMT", &expected_start));
  ASSERT_TRUE(base::Time::FromString("01 May 2022 00:00 GMT", &expected_end));
  fetch = calendar_utils::GetFetchStartEndTimes(date);
  EXPECT_EQ(fetch.first, expected_start);
  EXPECT_EQ(fetch.second, expected_end);

  // Timezone "America/Los_Angeles" is GMT - 7h.
  timezone_settings.SetTimezoneFromID(u"America/Los_Angeles");
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 00:00 GMT", &date));
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 07:00 GMT", &expected_start));
  ASSERT_TRUE(base::Time::FromString("01 May 2022 07:00 GMT", &expected_end));
  fetch = calendar_utils::GetFetchStartEndTimes(date);
  EXPECT_EQ(fetch.first, expected_start);
  EXPECT_EQ(fetch.second, expected_end);

  // The month is with DST ended date. The time difference is changed from -7h
  // to -8h.
  ASSERT_TRUE(base::Time::FromString("01 Nov 2022 00:00 GMT", &date));
  ASSERT_TRUE(base::Time::FromString("01 Nov 2022 07:00 GMT", &expected_start));
  ASSERT_TRUE(base::Time::FromString("01 Dec 2022 08:00 GMT", &expected_end));
  fetch = calendar_utils::GetFetchStartEndTimes(date);
  EXPECT_EQ(fetch.first, expected_start);
  EXPECT_EQ(fetch.second, expected_end);

  // Timezone "Asia/Bangkok" is GMT + 7h.
  timezone_settings.SetTimezoneFromID(u"Asia/Bangkok");
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 00:00 GMT", &date));
  ASSERT_TRUE(base::Time::FromString("31 Mar 2022 17:00 GMT", &expected_start));
  ASSERT_TRUE(base::Time::FromString("30 Apr 2022 17:00 GMT", &expected_end));
  fetch = calendar_utils::GetFetchStartEndTimes(date);
  EXPECT_EQ(fetch.first, expected_start);
  EXPECT_EQ(fetch.second, expected_end);
}

TEST_F(CalendarUtilsUnittest, MinMaxTime) {
  base::Time date_1;
  base::Time date_2;
  base::Time date_3;
  base::Time date_4;
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 00:00 GMT", &date_1));
  ASSERT_TRUE(base::Time::FromString("01 Apr 2023 00:00 GMT", &date_2));
  ASSERT_TRUE(base::Time::FromString("01 Apr 2022 20:00 GMT", &date_3));
  ASSERT_TRUE(base::Time::FromString("31 Mar 2022 00:00 GMT", &date_4));

  EXPECT_EQ(date_2, calendar_utils::GetMaxTime(date_1, date_2));
  EXPECT_EQ(date_3, calendar_utils::GetMaxTime(date_1, date_3));
  EXPECT_EQ(date_4, calendar_utils::GetMinTime(date_1, date_4));
}

}  // namespace ash
