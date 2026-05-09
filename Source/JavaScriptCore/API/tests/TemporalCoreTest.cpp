/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TemporalCoreTest.h"

#include "CalendarArithmetic.h"
#include "ISO8601.h"
#include "ISOArithmetic.h"
#include "InstantCore.h"
#include "Rounding.h"
#include "TemporalCoreTypes.h"
#include "TemporalEnums.h"
#include <stdio.h>
#include <wtf/Int128.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace TemporalCore {

// ---------------------------------------------------------------------------
// Assertion helpers
// ---------------------------------------------------------------------------

static int s_failures = 0;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#define TCHECK_EQ(actual, expected, name)                                                  \
    do {                                                                                   \
        if ((actual) != (expected)) {                                                      \
            fprintf(stderr, "FAIL [%s]: got %s, expected %s\n", name, #actual, #expected); \
            s_failures++;                                                                  \
        }                                                                                  \
    } while (0)

#define TCHECK_TRUE(cond, name)                                               \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "FAIL [%s]: condition false: %s\n", name, #cond); \
            s_failures++;                                                     \
        }                                                                     \
    } while (0)

#define TCHECK_ERR(result, name)                                                  \
    do {                                                                          \
        if ((result)) {                                                           \
            fprintf(stderr, "FAIL [%s]: expected error but got success\n", name); \
            s_failures++;                                                         \
        }                                                                         \
    } while (0)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

// ---------------------------------------------------------------------------
// ISOArithmetic tests — mirrors temporal_rs src/iso.rs tests
// ---------------------------------------------------------------------------

static void testBalanceISODate()
{
    // temporal_rs: test balance_iso_date
    // 2020-01-32 -> 2020-02-01
    auto r = balanceISODate(2020, 1, 32);
    TCHECK_EQ(r.year(), 2020, "balanceISODate: 2020-01-32 year");
    TCHECK_EQ(r.month(), 2u, "balanceISODate: 2020-01-32 month");
    TCHECK_EQ(r.day(), 1u, "balanceISODate: 2020-01-32 day");

    // 2020-12-32 -> 2021-01-01
    auto r2 = balanceISODate(2020, 12, 32);
    TCHECK_EQ(r2.year(), 2021, "balanceISODate: 2020-12-32 year");
    TCHECK_EQ(r2.month(), 1u, "balanceISODate: 2020-12-32 month");
    TCHECK_EQ(r2.day(), 1u, "balanceISODate: 2020-12-32 day");

    // 2020-01-00 -> 2019-12-31
    auto r3 = balanceISODate(2020, 1, 0);
    TCHECK_EQ(r3.year(), 2019, "balanceISODate: 2020-01-00 year");
    TCHECK_EQ(r3.month(), 12u, "balanceISODate: 2020-01-00 month");
    TCHECK_EQ(r3.day(), 31u, "balanceISODate: 2020-01-00 day");

    // 2020-03-01 (unchanged)
    auto r4 = balanceISODate(2020, 3, 1);
    TCHECK_EQ(r4.year(), 2020, "balanceISODate: 2020-03-01 year");
    TCHECK_EQ(r4.month(), 3u, "balanceISODate: 2020-03-01 month");
    TCHECK_EQ(r4.day(), 1u, "balanceISODate: 2020-03-01 day");
}

static void testRegulateISODate()
{
    // temporal_rs: test regulate_iso_date
    // constrain: 2020-02-30 -> 2020-02-29 (2020 is leap year)
    auto r = regulateISODate(2020, 2, 30, TemporalOverflow::Constrain);
    TCHECK_TRUE(r.has_value(), "regulateISODate: constrain 2020-02-30 ok");
    TCHECK_EQ(r->month(), 2u, "regulateISODate: constrain month");
    TCHECK_EQ(r->day(), 29u, "regulateISODate: constrain day");

    // constrain: 2021-02-30 -> 2021-02-28 (2021 not leap)
    auto r2 = regulateISODate(2021, 2, 30, TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "regulateISODate: constrain 2021-02-30 ok");
    TCHECK_EQ(r2->day(), 28u, "regulateISODate: constrain 2021-02-30 day");

    // reject: 2020-13-01 -> error
    auto r3 = regulateISODate(2020, 13, 1, TemporalOverflow::Reject);
    TCHECK_TRUE(!r3.has_value(), "regulateISODate: reject 2020-13-01 errors");

    // reject: valid 2020-06-15 -> ok
    auto r4 = regulateISODate(2020, 6, 15, TemporalOverflow::Reject);
    TCHECK_TRUE(r4.has_value(), "regulateISODate: reject 2020-06-15 ok");
    TCHECK_EQ(r4->year(), 2020, "regulateISODate: reject year");
    TCHECK_EQ(r4->month(), 6u, "regulateISODate: reject month");
    TCHECK_EQ(r4->day(), 15u, "regulateISODate: reject day");
}

static void testISODateAdd()
{
    // temporal_rs: plain_date.rs test simple_date_add
    // 1976-11-18 + P43Y -> 2019-11-18
    auto r1 = isoDateAdd({ 1976, 11, 18 }, ISO8601::Duration(43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "isoDateAdd: +43y ok");
    TCHECK_EQ(r1->year(), 2019, "isoDateAdd: +43y year");
    TCHECK_EQ(r1->month(), 11u, "isoDateAdd: +43y month");
    TCHECK_EQ(r1->day(), 18u, "isoDateAdd: +43y day");

    // 1976-11-18 + P3M -> 1977-02-18
    auto r2 = isoDateAdd({ 1976, 11, 18 }, ISO8601::Duration(0, 3, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "isoDateAdd: +3m ok");
    TCHECK_EQ(r2->year(), 1977, "isoDateAdd: +3m year");
    TCHECK_EQ(r2->month(), 2u, "isoDateAdd: +3m month");
    TCHECK_EQ(r2->day(), 18u, "isoDateAdd: +3m day");

    // 1976-11-18 + P20D -> 1976-12-08
    auto r3 = isoDateAdd({ 1976, 11, 18 }, ISO8601::Duration(0, 0, 0, 20, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "isoDateAdd: +20d ok");
    TCHECK_EQ(r3->year(), 1976, "isoDateAdd: +20d year");
    TCHECK_EQ(r3->month(), 12u, "isoDateAdd: +20d month");
    TCHECK_EQ(r3->day(), 8u, "isoDateAdd: +20d day");

    // 2019-11-18 - P43Y -> 1976-11-18
    auto r4 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(-43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r4.has_value(), "isoDateAdd: -43y ok");
    TCHECK_EQ(r4->year(), 1976, "isoDateAdd: -43y year");

    // constrain: 2021-01-31 + P1M -> 2021-02-28 (not 2021-02-31)
    auto r5 = isoDateAdd({ 2021, 1, 31 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r5.has_value(), "isoDateAdd: end-of-month constrain ok");
    TCHECK_EQ(r5->month(), 2u, "isoDateAdd: end-of-month month");
    TCHECK_EQ(r5->day(), 28u, "isoDateAdd: end-of-month day");
}

static void testISODateCompare()
{
    // temporal_rs: src/iso.rs IsoDate::cmp
    TCHECK_EQ(isoDateCompare({ 2020, 1, 1 }, { 2020, 1, 1 }), 0, "isoDateCompare: equal");
    TCHECK_EQ(isoDateCompare({ 2020, 1, 2 }, { 2020, 1, 1 }), 1, "isoDateCompare: later day");
    TCHECK_EQ(isoDateCompare({ 2020, 1, 1 }, { 2020, 1, 2 }), -1, "isoDateCompare: earlier day");
    TCHECK_EQ(isoDateCompare({ 2021, 1, 1 }, { 2020, 12, 31 }), 1, "isoDateCompare: later year");
    TCHECK_EQ(isoDateCompare({ 2020, 6, 1 }, { 2020, 7, 1 }), -1, "isoDateCompare: earlier month");
}

static void testDiffISODate()
{
    // temporal_rs: plain_date.rs test simple_date_until
    // 1969-07-24 until 1969-10-05 in days = 73
    auto r1 = diffISODate({ 1969, 7, 24 }, { 1969, 10, 5 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r1.days()), 73LL, "diffISODate: 73 days");

    // 1969-07-24 until 1996-03-03 in days = 9719
    auto r2 = diffISODate({ 1969, 7, 24 }, { 1996, 3, 3 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r2.days()), 9719LL, "diffISODate: 9719 days");

    // 1969-07-24 until 1969-10-05 in months = 2m12d
    auto r3 = diffISODate({ 1969, 7, 24 }, { 1969, 10, 5 }, TemporalUnit::Month);
    TCHECK_EQ(static_cast<int64_t>(r3.months()), 2LL, "diffISODate: months");
    TCHECK_EQ(static_cast<int64_t>(r3.days()), 11LL, "diffISODate: remaining days");

    // Same date -> zero
    auto r4 = diffISODate({ 2020, 6, 15 }, { 2020, 6, 15 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r4.days()), 0LL, "diffISODate: same date");

    // Negative diff: 1969-10-05 until 1969-07-24 in days = -73
    auto r5 = diffISODate({ 1969, 10, 5 }, { 1969, 7, 24 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r5.days()), -73LL, "diffISODate: negative days");
}

// ---------------------------------------------------------------------------
// Rounding tests — mirrors temporal_rs src/rounding.rs tests
// ---------------------------------------------------------------------------

static void testRoundNumberToIncrementDouble()
{
    // temporal_rs: round_number_to_increment tests
    // 5.5 with increment 1, HalfExpand -> 6
    TCHECK_EQ(roundNumberToIncrementDouble(5.5, 1.0, RoundingMode::HalfExpand), 6.0, "round: 5.5 HalfExpand");
    // 5.5 with increment 1, HalfTrunc -> 5
    TCHECK_EQ(roundNumberToIncrementDouble(5.5, 1.0, RoundingMode::HalfTrunc), 5.0, "round: 5.5 HalfTrunc");
    // 5.5 with increment 1, HalfEven -> 6 (round to even)
    TCHECK_EQ(roundNumberToIncrementDouble(5.5, 1.0, RoundingMode::HalfEven), 6.0, "round: 5.5 HalfEven");
    // 4.5 with increment 1, HalfEven -> 4 (round to even)
    TCHECK_EQ(roundNumberToIncrementDouble(4.5, 1.0, RoundingMode::HalfEven), 4.0, "round: 4.5 HalfEven");

    // Increment > 1: 23 / increment 5, Trunc -> 20
    TCHECK_EQ(roundNumberToIncrementDouble(23.0, 5.0, RoundingMode::Trunc), 20.0, "round: 23 inc5 Trunc");
    // 23 / increment 5, Ceil -> 25
    TCHECK_EQ(roundNumberToIncrementDouble(23.0, 5.0, RoundingMode::Ceil), 25.0, "round: 23 inc5 Ceil");
    // 23 / increment 5, Floor -> 20
    TCHECK_EQ(roundNumberToIncrementDouble(23.0, 5.0, RoundingMode::Floor), 20.0, "round: 23 inc5 Floor");

    // Negative: -23 / increment 5, Trunc -> -20
    TCHECK_EQ(roundNumberToIncrementDouble(-23.0, 5.0, RoundingMode::Trunc), -20.0, "round: -23 inc5 Trunc");
    // -23 / increment 5, Floor -> -25
    TCHECK_EQ(roundNumberToIncrementDouble(-23.0, 5.0, RoundingMode::Floor), -25.0, "round: -23 inc5 Floor");
}

static void testRoundNumberToIncrementInt128()
{
    // temporal_rs: round_number_to_increment (integer path)
    // 5 / inc 2, HalfExpand -> 6
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(5), Int128(2), RoundingMode::HalfExpand), Int128(6), "roundInt128: 5 inc2 HalfExpand");
    // 5 / inc 2, HalfTrunc -> 4
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(5), Int128(2), RoundingMode::HalfTrunc), Int128(4), "roundInt128: 5 inc2 HalfTrunc");
    // 4 / inc 2, HalfEven -> 4
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(4), Int128(2), RoundingMode::HalfEven), Int128(4), "roundInt128: 4 inc2 HalfEven");
    // 6 / inc 2, HalfEven -> 6
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(6), Int128(2), RoundingMode::HalfEven), Int128(6), "roundInt128: 6 inc2 HalfEven");

    // Nanoseconds: 1500000000 / inc 1000000000 (1s), HalfExpand -> 2000000000
    Int128 ns = Int128(1500000000LL);
    Int128 inc = Int128(1000000000LL);
    TCHECK_EQ(roundNumberToIncrementInt128(ns, inc, RoundingMode::HalfExpand), Int128(2000000000LL), "roundInt128: 1.5s HalfExpand");

    // temporal_rs: duration rounding 25h with inc=1day (86400s = 86400000000000ns)
    // 25h in ns = 90000000000000, inc = 86400000000000, Trunc -> 86400000000000
    Int128 h25 = Int128(90000000000000LL);
    Int128 day = Int128(86400000000000LL);
    TCHECK_EQ(roundNumberToIncrementInt128(h25, day, RoundingMode::Trunc), day, "roundInt128: 25h Trunc to 1day");
    // Floor same result
    TCHECK_EQ(roundNumberToIncrementInt128(h25, day, RoundingMode::Floor), day, "roundInt128: 25h Floor to 1day");
}

static void testMaximumRoundingIncrement()
{
    // temporal_rs: maximum_temporal_duration_rounding_increment
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Year).has_value(), "maxIncrement: Year = unlimited");
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Month).has_value(), "maxIncrement: Month = unlimited");
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Week).has_value(), "maxIncrement: Week = unlimited");
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Day).has_value(), "maxIncrement: Day = unlimited");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Hour).value_or(0), 24u, "maxIncrement: Hour = 24");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Minute).value_or(0), 60u, "maxIncrement: Minute = 60");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Second).value_or(0), 60u, "maxIncrement: Second = 60");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Millisecond).value_or(0), 1000u, "maxIncrement: Millisecond = 1000");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Microsecond).value_or(0), 1000u, "maxIncrement: Microsecond = 1000");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Nanosecond).value_or(0), 1000u, "maxIncrement: Nanosecond = 1000");
}

// ---------------------------------------------------------------------------
// CalendarArithmetic tests — mirrors temporal_rs src/builtins/core/calendar.rs
// ---------------------------------------------------------------------------

static void testCalendarDateAdd()
{
    // ISO8601 path only (no ICU needed)
    // temporal_rs: Calendar::date_add (iso8601)
    // 1976-11-18 + P43Y = 2019-11-18
    auto r1 = calendarDateAdd({ 1976, 11, 18 }, ISO8601::Duration(43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "calendarDateAdd: +43y ok");
    TCHECK_EQ(r1->year(), 2019, "calendarDateAdd: +43y year");

    // 1976-11-18 + P-43Y = 1933-11-18
    auto r2 = calendarDateAdd({ 1976, 11, 18 }, ISO8601::Duration(-43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "calendarDateAdd: -43y ok");
    TCHECK_EQ(r2->year(), 1933, "calendarDateAdd: -43y year");

    // End-of-month constrain: 2020-01-31 + P1M = 2020-02-29 (leap year)
    auto r3 = calendarDateAdd({ 2020, 1, 31 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "calendarDateAdd: eom constrain ok");
    TCHECK_EQ(r3->month(), 2u, "calendarDateAdd: eom month");
    TCHECK_EQ(r3->day(), 29u, "calendarDateAdd: eom day");

    // Weeks: 1976-11-18 + P2W = 1976-12-02
    auto r4 = calendarDateAdd({ 1976, 11, 18 }, ISO8601::Duration(0, 0, 2, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r4.has_value(), "calendarDateAdd: +2w ok");
    TCHECK_EQ(r4->month(), 12u, "calendarDateAdd: +2w month");
    TCHECK_EQ(r4->day(), 2u, "calendarDateAdd: +2w day");
}

static void testCalendarDateUntil()
{
    // ISO8601 path only — mirrors temporal_rs: Calendar::date_until + date_until_largest_year
    // 1969-07-24 until 1996-03-03 in days = 9719
    auto r1 = calendarDateUntil({ 1969, 7, 24 }, { 1996, 3, 3 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r1.days()), 9719LL, "calendarDateUntil: 9719 days");

    // Same date -> zero
    auto r2 = calendarDateUntil({ 2020, 6, 15 }, { 2020, 6, 15 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r2.days()), 0LL, "calendarDateUntil: same date");

    // 1969-07-24 until 1969-10-05 in months = 2m11d
    auto r3 = calendarDateUntil({ 1969, 7, 24 }, { 1969, 10, 5 }, TemporalUnit::Month);
    TCHECK_EQ(static_cast<int64_t>(r3.months()), 2LL, "calendarDateUntil: 2 months");

    // Negative: later until earlier
    auto r4 = calendarDateUntil({ 1996, 3, 3 }, { 1969, 7, 24 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r4.days()), -9719LL, "calendarDateUntil: -9719 days");

    // temporal_rs: date_until_largest_year — Year largestUnit
    // 2021-07-16 until 2022-07-16 = exactly 1 year
    auto r5 = calendarDateUntil({ 2021, 7, 16 }, { 2022, 7, 16 }, TemporalUnit::Year);
    TCHECK_EQ(static_cast<int64_t>(r5.years()), 1LL, "calendarDateUntil: 1 year");
    TCHECK_EQ(static_cast<int64_t>(r5.months()), 0LL, "calendarDateUntil: 1y months=0");
    TCHECK_EQ(static_cast<int64_t>(r5.days()), 0LL, "calendarDateUntil: 1y days=0");

    // 2021-07-16 until 2031-07-16 = exactly 10 years
    auto r6 = calendarDateUntil({ 2021, 7, 16 }, { 2031, 7, 16 }, TemporalUnit::Year);
    TCHECK_EQ(static_cast<int64_t>(r6.years()), 10LL, "calendarDateUntil: 10 years");

    // 2021-07-16 until 2022-07-19 = 1 year + 3 days
    auto r7 = calendarDateUntil({ 2021, 7, 16 }, { 2022, 7, 19 }, TemporalUnit::Year);
    TCHECK_EQ(static_cast<int64_t>(r7.years()), 1LL, "calendarDateUntil: 1y3d years");
    TCHECK_EQ(static_cast<int64_t>(r7.days()), 3LL, "calendarDateUntil: 1y3d days");
}

// ---------------------------------------------------------------------------
// ISO date limit tests — mirrors temporal_rs src/iso.rs limit tests
// ---------------------------------------------------------------------------

static void testISODateLimits()
{
    // Max valid ISO date for Temporal: +275760-09-13 (±1e8 epoch days)
    // balanceISODate returns outOfRangeYear only when year exceeds the year representation limit
    auto rMax = balanceISODate(275760, 9, 13);
    TCHECK_EQ(rMax.year(), 275760, "limits: max date year");
    TCHECK_EQ(rMax.month(), 9u, "limits: max date month");
    TCHECK_EQ(rMax.day(), 13u, "limits: max date day");

    // Min valid ISO date: -271821-04-19
    auto rMin = balanceISODate(-271821, 4, 19);
    TCHECK_EQ(rMin.year(), -271821, "limits: min date year");
    TCHECK_EQ(rMin.month(), 4u, "limits: min date month");
    TCHECK_EQ(rMin.day(), 19u, "limits: min date day");

    // Unix epoch: 1970-01-01
    auto rEpoch = balanceISODate(1970, 1, 1);
    TCHECK_EQ(rEpoch.year(), 1970, "limits: epoch year");
    TCHECK_EQ(rEpoch.month(), 1u, "limits: epoch month");
    TCHECK_EQ(rEpoch.day(), 1u, "limits: epoch day");

    // Day before epoch: 1969-12-31
    auto rEpochMinus1 = balanceISODate(1969, 12, 31);
    TCHECK_EQ(rEpochMinus1.year(), 1969, "limits: epoch-1 year");
    TCHECK_EQ(rEpochMinus1.month(), 12u, "limits: epoch-1 month");
    TCHECK_EQ(rEpochMinus1.day(), 31u, "limits: epoch-1 day");

    // isoDateAdd past Temporal max -> error (±1e8 days limit)
    // Max date + P1D exceeds the epoch day limit
    auto rOverMax = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOverMax.has_value(), "limits: isoDateAdd max+1d rejects");

    // Min date - P1D exceeds minimum
    auto rUnderMin = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rUnderMin.has_value(), "limits: isoDateAdd min-1d rejects");

    // regulateISODate validates month/day range (not Temporal epoch limits)
    auto rRegValid = regulateISODate(275760, 9, 13, TemporalOverflow::Reject);
    TCHECK_TRUE(rRegValid.has_value(), "limits: regulate 275760-09-13 ok");

    auto rRegBadMonth = regulateISODate(2020, 13, 1, TemporalOverflow::Reject);
    TCHECK_TRUE(!rRegBadMonth.has_value(), "limits: regulate month 13 rejects");
}

// ---------------------------------------------------------------------------
// Negative number rounding — mirrors temporal_rs src/rounding.rs tests
// ---------------------------------------------------------------------------

static void testNegativeRounding()
{
    // temporal_rs: test_basic_rounding_cases (negative values)
    // -101 / 10
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Ceil), -100.0, "negRound: -101 inc10 Ceil");
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Floor), -110.0, "negRound: -101 inc10 Floor");
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Expand), -110.0, "negRound: -101 inc10 Expand");
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Trunc), -100.0, "negRound: -101 inc10 Trunc");

    // -105 / 10 (midpoint)
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfCeil), -100.0, "negRound: -105 HalfCeil");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfFloor), -110.0, "negRound: -105 HalfFloor");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfExpand), -110.0, "negRound: -105 HalfExpand");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfTrunc), -100.0, "negRound: -105 HalfTrunc");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfEven), -100.0, "negRound: -105 HalfEven (even=10)");

    // -115 / 10: HalfEven -> -120 (even=12)
    TCHECK_EQ(roundNumberToIncrementDouble(-115.0, 10.0, RoundingMode::HalfEven), -120.0, "negRound: -115 HalfEven (even=12)");

    // -107 / 10 (not midpoint)
    TCHECK_EQ(roundNumberToIncrementDouble(-107.0, 10.0, RoundingMode::Ceil), -100.0, "negRound: -107 Ceil");
    TCHECK_EQ(roundNumberToIncrementDouble(-107.0, 10.0, RoundingMode::Floor), -110.0, "negRound: -107 Floor");

    // Int128 negative rounding
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Ceil), Int128(-100), "negRoundI128: Ceil");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Floor), Int128(-110), "negRoundI128: Floor");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfExpand), Int128(-110), "negRoundI128: HalfExpand midpoint");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfTrunc), Int128(-100), "negRoundI128: HalfTrunc midpoint");
}

// ---------------------------------------------------------------------------
// roundNumberToIncrementAsIfPositive — mirrors temporal_rs round_as_if_positive
// ---------------------------------------------------------------------------

static void testRoundAsIfPositive()
{
    // roundNumberToIncrementAsIfPositive always uses getUnsignedRoundingMode(mode, false).
    // For negative x=-107, increment=10: C++ quotient=-10, r1=-11, r2=-10.
    // Trunc->Zero->r1*inc = -110; Expand->Infinity->r2*inc = -100.
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Trunc), Int128(-110), "asIfPos: -107 Trunc=-110");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Expand), Int128(-100), "asIfPos: -107 Expand=-100");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Ceil), Int128(-100), "asIfPos: -107 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Floor), Int128(-110), "asIfPos: -107 Floor=-110");

    // Positive values: same as regular roundNumberToIncrementInt128
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(107), Int128(10), RoundingMode::Trunc), Int128(100), "asIfPos: 107 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(107), Int128(10), RoundingMode::Expand), Int128(110), "asIfPos: 107 Expand=110");

    // Zero is unchanged
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(0), Int128(10), RoundingMode::HalfExpand), Int128(0), "asIfPos: 0 = 0");
}

// ---------------------------------------------------------------------------
// negateTemporalRoundingMode — mirrors temporal_rs RoundingMode::negate
// ---------------------------------------------------------------------------

static void testNegateRoundingMode()
{
    // temporal_rs: RoundingMode::negate
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Floor)), static_cast<int>(RoundingMode::Ceil), "negate: Floor->Ceil");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Ceil)), static_cast<int>(RoundingMode::Floor), "negate: Ceil->Floor");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfFloor)), static_cast<int>(RoundingMode::HalfCeil), "negate: HalfFloor->HalfCeil");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfCeil)), static_cast<int>(RoundingMode::HalfFloor), "negate: HalfCeil->HalfFloor");
    // Symmetric modes unchanged
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Trunc)), static_cast<int>(RoundingMode::Trunc), "negate: Trunc unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Expand)), static_cast<int>(RoundingMode::Expand), "negate: Expand unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfExpand)), static_cast<int>(RoundingMode::HalfExpand), "negate: HalfExpand unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfTrunc)), static_cast<int>(RoundingMode::HalfTrunc), "negate: HalfTrunc unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfEven)), static_cast<int>(RoundingMode::HalfEven), "negate: HalfEven unchanged");
}

// ---------------------------------------------------------------------------
// isoDateAdd boundary/error cases
// ---------------------------------------------------------------------------

static void testISODateAddBoundaries()
{
    // temporal_rs: date_add_limits — adding to max date
    // Max date + P1D -> error (out of range)
    auto rOverMax = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOverMax.has_value(), "addBounds: max+1d rejects");

    // Min date - P1D -> error
    auto rUnderMin = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rUnderMin.has_value(), "addBounds: min-1d rejects");

    // Max date itself is valid
    auto rMax = isoDateAdd({ 275760, 9, 12 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rMax.has_value(), "addBounds: max date ok");
    TCHECK_EQ(rMax->year(), 275760, "addBounds: max year");
    TCHECK_EQ(rMax->day(), 13u, "addBounds: max day");

    // Constrain: exceed max -> constrain clamps
    auto rConstrain = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    // +1M from 275760-09-13 would be 275760-10-13 which exceeds max -> out of range
    TCHECK_TRUE(!rConstrain.has_value(), "addBounds: exceed max+1M errors");
}

// ---------------------------------------------------------------------------
// Negative number rounding — mirrors temporal_rs src/rounding.rs tests
// ---------------------------------------------------------------------------

static void testBalanceISOYearMonth()
{
    // Month overflow: 2021-13 -> 2022-01
    auto r1 = balanceISOYearMonth(2021, 13);
    TCHECK_EQ(r1.year(), 2022, "balanceYM: 2021m13 year");
    TCHECK_EQ(r1.month(), 1u, "balanceYM: 2021m13 month");
    // Month underflow: 2021-00 -> 2020-12
    auto r2 = balanceISOYearMonth(2021, 0);
    TCHECK_EQ(r2.year(), 2020, "balanceYM: 2021m0 year");
    TCHECK_EQ(r2.month(), 12u, "balanceYM: 2021m0 month");
    // No-op: 2020-06 -> 2020-06
    auto r3 = balanceISOYearMonth(2020, 6);
    TCHECK_EQ(r3.year(), 2020, "balanceYM: identity year");
    TCHECK_EQ(r3.month(), 6u, "balanceYM: identity month");
    // Large overflow: 2021-25 -> 2023-01
    auto r4 = balanceISOYearMonth(2021, 25);
    TCHECK_EQ(r4.year(), 2023, "balanceYM: 2021m25 year");
    TCHECK_EQ(r4.month(), 1u, "balanceYM: 2021m25 month");
}

static void testISOTimeCompare()
{
    // Equal
    TCHECK_EQ(isoTimeCompare({ 12, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 }), 0, "timeCompare: equal");
    // Hour difference
    TCHECK_EQ(isoTimeCompare({ 14, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 }), 1, "timeCompare: later hour");
    TCHECK_EQ(isoTimeCompare({ 12, 0, 0, 0, 0, 0 }, { 14, 0, 0, 0, 0, 0 }), -1, "timeCompare: earlier hour");
    // Nanosecond difference
    ISO8601::PlainTime t1(0, 0, 0, 0, 0, 1), t2(0, 0, 0, 0, 0, 0);
    TCHECK_EQ(isoTimeCompare(t1, t2), 1, "timeCompare: 1ns later");
    TCHECK_EQ(isoTimeCompare(t2, t1), -1, "timeCompare: 1ns earlier");
    // Max time vs min time
    ISO8601::PlainTime maxT(23, 59, 59, 999, 999, 999), minT(0, 0, 0, 0, 0, 0);
    TCHECK_EQ(isoTimeCompare(maxT, minT), 1, "timeCompare: max > min");
}

static void testApplyUnsignedRoundingMode()
{
    // x between r1 and r2 — direction modes
    TCHECK_EQ(applyUnsignedRoundingMode(1.3, 1.0, 2.0, UnsignedRoundingMode::Zero), 1.0, "applyURM: 1.3 Zero");
    TCHECK_EQ(applyUnsignedRoundingMode(1.3, 1.0, 2.0, UnsignedRoundingMode::Infinity), 2.0, "applyURM: 1.3 Inf");
    // x == r1 (exact lower bound)
    TCHECK_EQ(applyUnsignedRoundingMode(1.0, 1.0, 2.0, UnsignedRoundingMode::Zero), 1.0, "applyURM: exact=r1");
    // HalfZero at midpoint
    TCHECK_EQ(applyUnsignedRoundingMode(1.5, 1.0, 2.0, UnsignedRoundingMode::HalfZero), 1.0, "applyURM: 1.5 HalfZero");
    TCHECK_EQ(applyUnsignedRoundingMode(1.5, 1.0, 2.0, UnsignedRoundingMode::HalfInfinity), 2.0, "applyURM: 1.5 HalfInf");
    // HalfEven: 2.5 -> 2 (even lower), 3.5 -> 4 (even upper)
    TCHECK_EQ(applyUnsignedRoundingMode(2.5, 2.0, 3.0, UnsignedRoundingMode::HalfEven), 2.0, "applyURM: 2.5 HalfEven->2");
    TCHECK_EQ(applyUnsignedRoundingMode(3.5, 3.0, 4.0, UnsignedRoundingMode::HalfEven), 4.0, "applyURM: 3.5 HalfEven->4");
}

static void testMaximumInstantIncrement()
{
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Hour), 24.0, "maxInstInc: Hour=24");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Minute), 1440.0, "maxInstInc: Minute=1440");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Second), 86400.0, "maxInstInc: Second=86400");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Millisecond), 8.64e7, "maxInstInc: Ms");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Microsecond), 8.64e10, "maxInstInc: µs");
}

// ---------------------------------------------------------------------------
// validateTemporalRoundingIncrement
// ---------------------------------------------------------------------------

static void testValidateTemporalRoundingIncrement()
{
    // temporal_rs: RoundingIncrement::validate
    // Valid: increment=1, dividend=60 (exclusive)
    auto r1 = validateTemporalRoundingIncrement(1.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(r1.has_value(), "validate: 1 of 60 ok");

    // Valid: increment=5, dividend=60 (exclusive) — 60/5=12 integer
    auto r2 = validateTemporalRoundingIncrement(5.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(r2.has_value(), "validate: 5 of 60 ok");

    // Valid: increment=60, dividend=60 (inclusive)
    auto r3 = validateTemporalRoundingIncrement(60.0, 60.0, Inclusivity::Inclusive);
    TCHECK_TRUE(r3.has_value(), "validate: 60 of 60 inclusive ok");

    // Invalid: increment=61, dividend=60 (exclusive) — exceeds max
    auto r4 = validateTemporalRoundingIncrement(61.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!r4.has_value(), "validate: 61 of 60 rejects");

    // Invalid: increment=60, dividend=60 (exclusive) — equal not allowed exclusive
    auto r5 = validateTemporalRoundingIncrement(60.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!r5.has_value(), "validate: 60 of 60 exclusive rejects");

    // Invalid: increment=7, dividend=60 (exclusive) — not a divisor
    auto r6 = validateTemporalRoundingIncrement(7.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!r6.has_value(), "validate: 7 of 60 not divisor rejects");

    // No dividend: any positive increment ok
    auto r7 = validateTemporalRoundingIncrement(100.0, std::nullopt, Inclusivity::Exclusive);
    TCHECK_TRUE(r7.has_value(), "validate: no dividend ok");
}

// ---------------------------------------------------------------------------
// Comprehensive rounding: all 9 modes × positive/negative/midpoint
// Directly ports temporal_rs rounding.rs test_basic_rounding_cases +
// test_float_rounding_cases
// ---------------------------------------------------------------------------

static void testRoundingComprehensive()
{
    // --- Integer x=101, increment=10 (x not at midpoint, closer to 100) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Ceil), Int128(110), "comp: 101 Ceil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Floor), Int128(100), "comp: 101 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Expand), Int128(110), "comp: 101 Expand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Trunc), Int128(100), "comp: 101 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfCeil), Int128(100), "comp: 101 HalfCeil=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfFloor), Int128(100), "comp: 101 HalfFloor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfExpand), Int128(100), "comp: 101 HalfExpand=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfTrunc), Int128(100), "comp: 101 HalfTrunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfEven), Int128(100), "comp: 101 HalfEven=100");

    // --- Integer x=105, increment=10 (exactly at midpoint) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Ceil), Int128(110), "comp: 105 Ceil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Floor), Int128(100), "comp: 105 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Expand), Int128(110), "comp: 105 Expand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Trunc), Int128(100), "comp: 105 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfCeil), Int128(110), "comp: 105 HalfCeil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfFloor), Int128(100), "comp: 105 HalfFloor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfExpand), Int128(110), "comp: 105 HalfExpand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfTrunc), Int128(100), "comp: 105 HalfTrunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfEven), Int128(100), "comp: 105 HalfEven=100 (even=10)");

    // --- Integer x=107, increment=10 (closer to 110) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Ceil), Int128(110), "comp: 107 Ceil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Floor), Int128(100), "comp: 107 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Expand), Int128(110), "comp: 107 Expand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Trunc), Int128(100), "comp: 107 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::HalfExpand), Int128(110), "comp: 107 HalfExpand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::HalfTrunc), Int128(110), "comp: 107 HalfTrunc=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::HalfEven), Int128(110), "comp: 107 HalfEven=110");

    // --- Negative x=-101, increment=10 ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Ceil), Int128(-100), "comp: -101 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Floor), Int128(-110), "comp: -101 Floor=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Expand), Int128(-110), "comp: -101 Expand=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Trunc), Int128(-100), "comp: -101 Trunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::HalfExpand), Int128(-100), "comp: -101 HalfExpand=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::HalfTrunc), Int128(-100), "comp: -101 HalfTrunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::HalfEven), Int128(-100), "comp: -101 HalfEven=-100");

    // --- Negative x=-105, increment=10 (midpoint) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::Ceil), Int128(-100), "comp: -105 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::Floor), Int128(-110), "comp: -105 Floor=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfCeil), Int128(-100), "comp: -105 HalfCeil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfFloor), Int128(-110), "comp: -105 HalfFloor=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfEven), Int128(-100), "comp: -105 HalfEven=-100 (even=10)");

    // --- Small increment x=-9, increment=2 ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::Ceil), Int128(-8), "comp: -9 inc2 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::Floor), Int128(-10), "comp: -9 inc2 Floor=-10");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::HalfExpand), Int128(-10), "comp: -9 inc2 HalfExpand=-10");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::HalfTrunc), Int128(-8), "comp: -9 inc2 HalfTrunc=-8");

    // --- Float: -8.5, increment=1 (from temporal_rs test_float_rounding_cases) ---
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Ceil), -8.0, "float: -8.5 inc1 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Floor), -9.0, "float: -8.5 inc1 Floor=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Expand), -9.0, "float: -8.5 inc1 Expand=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Trunc), -8.0, "float: -8.5 inc1 Trunc=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfCeil), -8.0, "float: -8.5 inc1 HalfCeil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfFloor), -9.0, "float: -8.5 inc1 HalfFloor=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfExpand), -9.0, "float: -8.5 inc1 HalfExpand=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfTrunc), -8.0, "float: -8.5 inc1 HalfTrunc=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfEven), -8.0, "float: -8.5 inc1 HalfEven=-8 (even)");

    // --- Float: -8.5, increment=2 ---
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::Ceil), -8.0, "float: -8.5 inc2 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::Floor), -10.0, "float: -8.5 inc2 Floor=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::HalfCeil), -8.0, "float: -8.5 inc2 HalfCeil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::HalfFloor), -8.0, "float: -8.5 inc2 HalfFloor=-8 (even)");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::HalfEven), -8.0, "float: -8.5 inc2 HalfEven=-8 (even)");

    // --- Float: -9.5, increment=2 ---
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Ceil), -8.0, "float: -9.5 inc2 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Floor), -10.0, "float: -9.5 inc2 Floor=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Expand), -10.0, "float: -9.5 inc2 Expand=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Trunc), -8.0, "float: -9.5 inc2 Trunc=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfCeil), -10.0, "float: -9.5 inc2 HalfCeil=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfFloor), -10.0, "float: -9.5 inc2 HalfFloor=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfExpand), -10.0, "float: -9.5 inc2 HalfExpand=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfTrunc), -10.0, "float: -9.5 inc2 HalfTrunc=-10 (not midpoint)");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfEven), -10.0, "float: -9.5 inc2 HalfEven=-10 (even=5)");

    // --- Large nanosecond value from temporal_rs dt_since_basic_rounding ---
    // -84082624864197532 ns, increment=1800000000000 (30 min), HalfExpand
    TCHECK_EQ(
        roundNumberToIncrementInt128(Int128(-84082624864197532LL), Int128(1800000000000LL), RoundingMode::HalfExpand),
        Int128(-84083400000000000LL),
        "comp: large ns HalfExpand");
}

// ---------------------------------------------------------------------------
// iso.rs exact epoch-day boundary values
// temporal_rs: iso_date_to_epoch_days_limits + test_month_limits
// ---------------------------------------------------------------------------

static void testISOEpochDayLimits()
{
    // temporal_rs: iso_date_to_epoch_days_limits
    // -271821-04-20 = abs(days) exactly 100_000_000 (= MAX_DAYS_BASE)
    // isoDateAdd must succeed (within range)
    auto rMin20 = isoDateAdd({ -271821, 4, 20 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMin20.has_value(), "epochDays: -271821-04-20 is valid");

    // -271821-04-19 = abs(days) = MAX_DAYS_BASE + 1 -> valid Temporal lower bound
    auto rMin19 = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMin19.has_value(), "epochDays: -271821-04-19 is valid lower bound");

    // -271821-04-18 = abs(days) = MAX_DAYS_BASE + 2 -> out of range
    auto rMin18 = isoDateAdd({ -271821, 4, 18 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rMin18.has_value(), "epochDays: -271821-04-18 is out of range");

    // 275760-09-13 = abs(days) = MAX_DAYS_BASE (valid upper bound)
    auto rMax13 = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMax13.has_value(), "epochDays: 275760-09-13 is valid upper bound");

    // 275760-09-14 = abs(days) = MAX_DAYS_BASE + 1 -> out of range
    auto rMax14 = isoDateAdd({ 275760, 9, 14 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rMax14.has_value(), "epochDays: 275760-09-14 is out of range");
}

// ---------------------------------------------------------------------------
// rounding.rs: exact-multiple cases (x=100, inc=10) and (-100) and (-14, 3)
// These complete the test_basic_rounding_cases table
// ---------------------------------------------------------------------------

static void testRoundingExactMultiples()
{
    // temporal_rs: x=100 inc=10 -> all modes return 100 (exact multiple)
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Ceil), Int128(100), "exact: 100 Ceil=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Floor), Int128(100), "exact: 100 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Expand), Int128(100), "exact: 100 Expand=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Trunc), Int128(100), "exact: 100 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfCeil), Int128(100), "exact: 100 HalfCeil=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfFloor), Int128(100), "exact: 100 HalfFloor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfExpand), Int128(100), "exact: 100 HalfExpand=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfTrunc), Int128(100), "exact: 100 HalfTrunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfEven), Int128(100), "exact: 100 HalfEven=100");

    // Same for float
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Ceil), 100.0, "exactF: 100 Ceil=100");
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Floor), 100.0, "exactF: 100 Floor=100");
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Expand), 100.0, "exactF: 100 Expand=100");
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Trunc), 100.0, "exactF: 100 Trunc=100");

    // temporal_rs: x=-100 inc=10 -> all modes return -100
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Ceil), Int128(-100), "exact: -100 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Floor), Int128(-100), "exact: -100 Floor=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Expand), Int128(-100), "exact: -100 Expand=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Trunc), Int128(-100), "exact: -100 Trunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfCeil), Int128(-100), "exact: -100 HalfCeil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfFloor), Int128(-100), "exact: -100 HalfFloor=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfExpand), Int128(-100), "exact: -100 HalfExpand=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfTrunc), Int128(-100), "exact: -100 HalfTrunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfEven), Int128(-100), "exact: -100 HalfEven=-100");

    // temporal_rs: x=-14, inc=3 (non-exact, between -15 and -12, closer to -15)
    // -14 / 3: floor = -5 -> -15; ceil = -4 -> -12; remainder = (-14) mod 3 = 1 (one away from -15)
    // midpoint of 3 = 1.5, so 1 < 1.5 -> closer to floor (-15)
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Ceil), Int128(-12), "14/3: Ceil=-12");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Floor), Int128(-15), "14/3: Floor=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Expand), Int128(-15), "14/3: Expand=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Trunc), Int128(-12), "14/3: Trunc=-12");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfCeil), Int128(-15), "14/3: HalfCeil=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfFloor), Int128(-15), "14/3: HalfFloor=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfExpand), Int128(-15), "14/3: HalfExpand=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfTrunc), Int128(-15), "14/3: HalfTrunc=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfEven), Int128(-15), "14/3: HalfEven=-15");
}

// ---------------------------------------------------------------------------
// plain_date.rs: simple_date_subtract + new_date_limits + rounding_increment_observed
// ---------------------------------------------------------------------------

static void testDateSubtract()
{
    // temporal_rs: simple_date_subtract — 2019-11-18 - P43Y = 1976-11-18
    auto r1 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(-43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "subtract: -43y ok");
    TCHECK_EQ(r1->year(), 1976, "subtract: -43y year");
    TCHECK_EQ(r1->month(), 11u, "subtract: -43y month");
    TCHECK_EQ(r1->day(), 18u, "subtract: -43y day");

    // 2019-11-18 - P11M = 2018-12-18
    auto r2 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(0, -11, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "subtract: -11m ok");
    TCHECK_EQ(r2->year(), 2018, "subtract: -11m year");
    TCHECK_EQ(r2->month(), 12u, "subtract: -11m month");
    TCHECK_EQ(r2->day(), 18u, "subtract: -11m day");

    // 2019-11-18 - P20D = 2019-10-29
    auto r3 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(0, 0, 0, -20, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "subtract: -20d ok");
    TCHECK_EQ(r3->year(), 2019, "subtract: -20d year");
    TCHECK_EQ(r3->month(), 10u, "subtract: -20d month");
    TCHECK_EQ(r3->day(), 29u, "subtract: -20d day");
}

static void testNewDateLimits()
{
    // temporal_rs: new_date_limits — min valid = -271821-04-19, max valid = 275760-09-13
    // Just below min: -271821-04-18 -> out of range
    auto rErr1 = isoDateAdd({ -271821, 4, 18 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rErr1.has_value(), "newDateLimits: -271821-04-18 rejected");

    // Just above max: 275760-09-14 -> out of range
    auto rErr2 = isoDateAdd({ 275760, 9, 14 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rErr2.has_value(), "newDateLimits: 275760-09-14 rejected");

    // Exact boundaries are valid
    auto rOk1 = regulateISODate(-271821, 4, 19, TemporalOverflow::Reject);
    TCHECK_TRUE(rOk1.has_value(), "newDateLimits: -271821-04-19 ok");
    TCHECK_EQ(rOk1->year(), -271821, "newDateLimits: min year");
    TCHECK_EQ(rOk1->month(), 4u, "newDateLimits: min month");
    TCHECK_EQ(rOk1->day(), 19u, "newDateLimits: min day");

    auto rOk2 = regulateISODate(275760, 9, 13, TemporalOverflow::Reject);
    TCHECK_TRUE(rOk2.has_value(), "newDateLimits: 275760-09-13 ok");
    TCHECK_EQ(rOk2->year(), 275760, "newDateLimits: max year");
    TCHECK_EQ(rOk2->month(), 9u, "newDateLimits: max month");
    TCHECK_EQ(rOk2->day(), 13u, "newDateLimits: max day");

    // 275760-09-13 + 1D -> over max
    auto rOver = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOver.has_value(), "newDateLimits: max+1d rejected");

    // -271821-04-19 - 1D -> under min
    auto rUnder = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rUnder.has_value(), "newDateLimits: min-1d rejected");

    // 275760-09-12 + 1D -> exactly max, valid
    auto rExact = isoDateAdd({ 275760, 9, 12 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rExact.has_value(), "newDateLimits: 275760-09-12+1d=max ok");
    TCHECK_EQ(rExact->day(), 13u, "newDateLimits: 275760-09-12+1d day=13");

    // -271821-04-20 - 1D -> exactly min, valid
    auto rExact2 = isoDateAdd({ -271821, 4, 20 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rExact2.has_value(), "newDateLimits: -271821-04-20-1d=min ok");
    TCHECK_EQ(rExact2->day(), 19u, "newDateLimits: -271821-04-20-1d day=19");
}

static void testDateRoundingIncrement()
{
    // temporal_rs: rounding_increment_observed — date since with rounding increments
    // 2021-09-07 since 2019-01-08, smallest=Year, inc=4, HalfExpand -> 4 years
    // Actual diff ≈ 2.66 years -> rounds to 4 (nearest multiple of 4)
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Year);
        // 2y 7m 29d ≈ 2.66y, rounded to inc=4 -> 4
        double years = diff.years() + diff.months() / 12.0;
        double roundedYears = roundNumberToIncrementDouble(years, 4.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedYears), 4LL, "dateRoundInc: years inc=4 HalfExpand=4");
    }

    // smallest=Month, inc=10, HalfExpand -> 30 months (≈32m -> nearest 10 is 30)
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Month);
        // 32 months -> rounded to inc=10 -> 30
        double months = diff.months();
        double roundedMonths = roundNumberToIncrementDouble(months, 10.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedMonths), 30LL, "dateRoundInc: months inc=10 HalfExpand=30");
    }

    // smallest=Week, inc=12, HalfExpand -> 144 weeks
    // temporal_rs: rounding_increment_observed — Week case
    // 2019-01-08 to 2021-09-07 = 973 days = 139 weeks + 0 days
    // 139 / 12 = 11.583... -> HalfExpand -> 12 -> 144 weeks
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Week);
        // 973 days = 139 weeks exactly (973 = 139 * 7)
        TCHECK_EQ(static_cast<int64_t>(diff.weeks()), 139LL, "dateRoundInc: weeks=139");
        double weeks = diff.weeks();
        double roundedWeeks = roundNumberToIncrementDouble(weeks, 12.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedWeeks), 144LL, "dateRoundInc: weeks inc=12 HalfExpand=144");
    }

    // smallest=Day, inc=100, HalfExpand -> 1000 days
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Day);
        // 973 days -> 1000 (nearest 100)
        double days = diff.days();
        double roundedDays = roundNumberToIncrementDouble(days, 100.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedDays), 1000LL, "dateRoundInc: days inc=100 HalfExpand=1000");
    }
}

// Note: testInvalidDateStrings and testCriticalUnknownAnnotation use
// ISO8601::parseCalendarDateTime — confirmed JS_EXPORT_PRIVATE in ISO8601.h.

// ---------------------------------------------------------------------------
// invalid_strings — mirrors temporal_rs plain_date.rs test invalid_strings
// test262/test/built-ins/Temporal/Calendar/prototype/month/argument-string-invalid.js
// Tests that parseCalendarDateTime rejects invalid/unsupported ISO 8601 strings.
// Note: "2020-01-01[u-ca=notexist]" is NOT tested here because it parses
// successfully at the C++ layer; the calendar validation happens in JS.
// ---------------------------------------------------------------------------

static void testInvalidDateStrings()
{
    // temporal_rs: plain_date.rs test invalid_strings
    // All of these must return std::nullopt from parseCalendarDateTime.
    static const char* const invalidStrings[] = {
        // Completely invalid
        "",
        "invalid iso8601",
        // Day out of range
        "2020-01-00",
        "2020-01-32",
        "2020-02-30",
        "2021-02-29",
        // Month out of range
        "2020-00-01",
        "2020-13-01",
        // Trailing separator with no time
        "2020-01-01T",
        // Time fields out of range
        "2020-01-01T25:00:00",
        "2020-01-01T01:60:00",
        "2020-01-01T01:60:61",
        // Trailing junk
        "2020-01-01junk",
        "2020-01-01T00:00:00junk",
        "2020-01-01T00:00:00+00:00junk",
        "2020-01-01T00:00:00+00:00[UTC]junk",
        "2020-01-01T00:00:00+00:00[UTC][u-ca=iso8601]junk",
        // Non-standard year widths / formats
        "02020-01-01",
        "2020-001-01",
        "2020-01-001",
        "2020-01-01T001",
        "2020-01-01T01:001",
        "2020-01-01T01:01:001",
        // Unsupported formats (week/ordinal)
        "2020-W01-1",
        "2020-001",
        "+0002020-01-01",
        // Too-short date (no day for Date format)
        "2020-01",
        "+002020-01",
        "01-01",
        "2020-W01",
        // Duration strings (not dates)
        "P1Y",
        "-P12Y",
        // Too many fractional second digits
        "1970-01-01T00:00:00.1234567891",
        "1970-01-01T00:00:00.1234567890",
    };
    for (auto* s : invalidStrings) {
        auto r = ISO8601::parseCalendarDateTime(StringView::fromLatin1(s), TemporalDateFormat::Date);
        TCHECK_TRUE(!r.has_value(), "invalidString: should reject");
    }
}

// ---------------------------------------------------------------------------
// argument_string_critical_unknown_annotation — mirrors temporal_rs plain_date.rs
// test262: argument-string-critical-unknown-annotation.js
// Strings with critical (!) unknown annotations must fail parsing.
// ---------------------------------------------------------------------------

static void testCriticalUnknownAnnotation()
{
    // temporal_rs: plain_date.rs test argument_string_critical_unknown_annotation
    static const char* const criticalAnnotationStrings[] = {
        "1970-01-01[!foo=bar]",
        "1970-01-01T00:00[!foo=bar]",
        "1970-01-01T00:00[UTC][!foo=bar]",
        "1970-01-01T00:00[u-ca=iso8601][!foo=bar]",
        "1970-01-01T00:00[UTC][!foo=bar][u-ca=iso8601]",
        "1970-01-01T00:00[foo=bar][!_foo-bar0=Dont-Ignore-This-99999999999]",
    };
    for (auto* s : criticalAnnotationStrings) {
        auto r = ISO8601::parseCalendarDateTime(StringView::fromLatin1(s), TemporalDateFormat::Date);
        TCHECK_TRUE(!r.has_value(), "criticalAnnotation: should reject");
    }
}

// ---------------------------------------------------------------------------
// Section 1: direct temporal_rs ports
// ---------------------------------------------------------------------------

static void runTemporalRSTests()
{
    // --- iso.rs ---
    testISOEpochDayLimits(); // temporal_rs: iso_date_to_epoch_days_limits, test_month_limits

    // --- rounding.rs ---
    testMaximumRoundingIncrement(); // temporal_rs: (MaximumTemporalDurationRoundingIncrement)
    testNegateRoundingMode(); // temporal_rs: RoundingMode::negate
    testApplyUnsignedRoundingMode(); // temporal_rs: apply_unsigned_rounding_mode
    testRoundNumberToIncrementDouble(); // temporal_rs: test_basic_rounding_cases (double)
    testRoundNumberToIncrementInt128(); // temporal_rs: test_basic_rounding_cases (i128)
    testRoundingExactMultiples(); // temporal_rs: test_basic_rounding_cases (exact multiples)
    testRoundAsIfPositive(); // temporal_rs: round_as_if_positive
    testRoundingComprehensive(); // temporal_rs: test_basic_rounding_cases, test_float_rounding_cases, dt_since_basic_rounding
    testValidateTemporalRoundingIncrement(); // temporal_rs: RoundingIncrement::validate

    // --- plain_date.rs ---
    testBalanceISODate(); // temporal_rs: (balanceISODate)
    testBalanceISOYearMonth(); // temporal_rs: (balanceISOYearMonth)
    testRegulateISODate(); // temporal_rs: (regulateISODate)
    testISODateAdd(); // temporal_rs: simple_date_add
    testDateSubtract(); // temporal_rs: simple_date_subtract
    testISODateAddBoundaries(); // temporal_rs: date_add_limits
    testNewDateLimits(); // temporal_rs: new_date_limits
    testISODateCompare(); // temporal_rs: (isoDateCompare)
    testISOTimeCompare(); // temporal_rs: (isoTimeCompare)
    testDiffISODate(); // temporal_rs: simple_date_until, simple_date_since
    testDateRoundingIncrement(); // temporal_rs: rounding_increment_observed
    testInvalidDateStrings(); // temporal_rs: invalid_strings
    testCriticalUnknownAnnotation(); // temporal_rs: argument_string_critical_unknown_annotation
}

// ---------------------------------------------------------------------------
// Section 2: additional stress tests beyond temporal_rs
// ---------------------------------------------------------------------------

static void runStressTests()
{
    // ISO arithmetic stress
    testISODateLimits(); // Temporal epoch limit boundary checks
    testNegativeRounding(); // Negative number rounding across all modes

    // Calendar helpers
    testCalendarDateAdd(); // ISO calendarDateAdd
    testCalendarDateUntil(); // ISO calendarDateUntil

    // Instant
    testMaximumInstantIncrement(); // maximumInstantIncrement per unit
}

} // namespace TemporalCore
} // namespace JSC

// ---------------------------------------------------------------------------
// Entry point (extern "C" so testapi.c can call it)
// ---------------------------------------------------------------------------

extern "C" int testTemporalCore()
{
    using namespace JSC::TemporalCore;
    s_failures = 0;
    runTemporalRSTests();
    runStressTests();
    if (s_failures)
        fprintf(stderr, "testTemporalCore: %d test(s) FAILED\n", s_failures);
    else
        fprintf(stderr, "testTemporalCore: all tests passed\n");
    return s_failures ? 1 : 0;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
