/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TimeUtils.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include <iomanip>

namespace Warhead::TimeDiff // in us
{
    constexpr uint64 MILLISECONDS = 1000;
    constexpr uint64 SECONDS = 1000 * MILLISECONDS;
    constexpr uint64 MINUTES = 60 * SECONDS;
    constexpr uint64 HOURS = 60 * MINUTES;
    constexpr uint64 DAYS = 24 * HOURS;
}

Seconds Warhead::Time::TimeStringTo(std::string_view timestring)
{
    if (timestring.starts_with('-'))
        return Seconds(std::numeric_limits<int>::max());

    Seconds secs = 0s;
    Seconds buffer = 0s;
    Seconds multiplier = 0s;

    for (char const& itr : timestring)
    {
        if (itr == ' ')
            continue;

        if (std::isdigit(itr))
        {
            buffer *= 10;
            buffer += Seconds(itr - '0');
            continue;
        }

        switch (itr)
        {
            case 'd':
                multiplier = 1_days;
                break;
            case 'h':
                multiplier = 1h;
                break;
            case 'm':
                multiplier = 1min;
                break;
            case 's':
                multiplier = 1s;
                break;
            default:
                return 0s; // bad format
        }

        buffer *= multiplier.count();
        secs += buffer;
        buffer = 0s;
    }

    return secs;
}

std::string Warhead::Time::ToTimeString(Microseconds durationTime, uint8 outCount /*= 3*/, TimeFormat timeFormat /*= TimeFormat::ShortText*/)
{
    uint64 microsecs = durationTime.count() % 1000;
    uint64 millisecs = (durationTime.count() / TimeDiff::MILLISECONDS) % 1000;
    uint64 secs = (durationTime.count() / TimeDiff::SECONDS) % 60;
    uint64 minutes = (durationTime.count() / TimeDiff::MINUTES) % 60;
    uint64 hours = (durationTime.count() / TimeDiff::HOURS) % 24;
    uint64 days = durationTime.count() / TimeDiff::DAYS;

    std::string out;
    uint8 count = 0;
    bool isFirst = false;

    if (timeFormat == TimeFormat::Numeric)
    {
        auto AddOutNumerlic = [&isFirst, &out, &count, outCount](uint64 time)
        {
            if (count >= outCount)
                return;

            if (!isFirst)
            {
                isFirst = true;
                out.append(Warhead::StringFormat("{}:", time));
            }
            else
                out.append(Warhead::StringFormat("{:02}:", time));

            count++;
        };

        if (days)
            AddOutNumerlic(days);

        if (hours)
            AddOutNumerlic(hours);

        if (minutes)
            AddOutNumerlic(minutes);

        if (secs)
            AddOutNumerlic(secs);

        if (millisecs)
            AddOutNumerlic(millisecs);

        if (microsecs)
            AddOutNumerlic(microsecs);

        // Delete last (:)
        if (!out.empty())
            out.erase(out.end() - 1, out.end());

        return out;
    }

    auto AddOut = [&out, &count, timeFormat, outCount](uint32 timeCount, std::string_view shortText, std::string_view fullText1, std::string_view fullText)
    {
        if (count >= outCount)
            return;

        out.append(Warhead::ToString(timeCount));

        switch (timeFormat)
        {
            case TimeFormat::ShortText:
                out.append(shortText);
                break;
            case TimeFormat::FullText:
                out.append(timeCount == 1 ? fullText1 : fullText);
                break;
            default:
                out.append("<Unknown time format>");
        }

        count++;
    };

    if (days)
        AddOut(days, "d ", " Day ", " Days ");

    if (hours)
        AddOut(hours, "h ", " Hour ", " Hours ");

    if (minutes)
        AddOut(minutes, "m ", " Minute ", " Minutes ");

    if (secs)
        AddOut(secs, "s ", " Second ", " Seconds ");

    if (millisecs)
        AddOut(millisecs, "ms ", " Millisecond ", " Milliseconds ");

    if (microsecs)
        AddOut(microsecs, "us ", " Microsecond ", " Microseconds ");

    if (!out.empty())
        out.pop_back();

    return out;
}

#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
struct tm* localtime_r(time_t const* time, struct tm* result)
{
    localtime_s(result, time);
    return result;
}
#endif

std::tm Warhead::Time::TimeBreakdown(time_t time /*= 0*/)
{
    if (!time)
        time = GetEpochTime().count();

    std::tm timeLocal{};
    localtime_r(&time, &timeLocal);
    return timeLocal;
}

time_t Warhead::Time::LocalTimeToUTCTime(time_t time)
{
#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
    return time + _timezone;
#else
    return time + timezone;
#endif
}

std::string Warhead::Time::TimeToTimestampStr(Seconds time /*= 0s*/, std::string_view fmt /*= {}*/)
{
    std::stringstream ss;
    std::string format{ fmt };
    time_t t = time.count();

    if (format.empty())
    {
        format = "%Y-%m-%d %X";
    }

    ss << std::put_time(std::localtime(&t), format.c_str());
    return ss.str();
}

std::string Warhead::Time::TimeToHumanReadable(Seconds time /*= 0s*/, std::string_view fmt /*= {}*/)
{
    std::stringstream ss;
    std::string format{ fmt };
    time_t t = time.count();

    if (format.empty())
    {
        format = "%a %b %d %Y %X";
    }

    ss << std::put_time(std::localtime(&t), format.c_str());
    return ss.str();
}

uint32 Warhead::Time::GetSeconds(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_sec;
}

uint32 Warhead::Time::GetMinutes(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_min;
}

uint32 Warhead::Time::GetHours(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_hour;
}

uint32 Warhead::Time::GetDayInWeek(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_wday;
}

uint32 Warhead::Time::GetDayInMonth(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_mday;
}

uint32 Warhead::Time::GetDayInYear(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_yday;
}

uint32 Warhead::Time::GetMonth(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_mon;
}

uint32 Warhead::Time::GetYear(Seconds time /*= 0s*/)
{
    if (time == 0s)
        time = GetEpochTime();

    return TimeBreakdown(time.count()).tm_year;
}
