// Copyright James Joslin. All Rights Reserved.

#include "WeatherDateTime.h"

bool FWeatherDateTime::ToDateTime(FDateTime& OutDateTime) const
{
	if (!FDateTime::Validate(Year, Month, Day, Hour, Minute, Second, 0))
	{
		return false;
	}

	OutDateTime = FDateTime(Year, Month, Day, Hour, Minute, Second);
	return true;
}

FWeatherDateTime FWeatherDateTime::FromDateTime(const FDateTime& DateTime)
{
	FWeatherDateTime Result;
	Result.Year = DateTime.GetYear();
	Result.Month = DateTime.GetMonth();
	Result.Day = DateTime.GetDay();
	Result.Hour = DateTime.GetHour();
	Result.Minute = DateTime.GetMinute();
	Result.Second = DateTime.GetSecond();
	return Result;
}

bool FWeatherDateTime::IsValid() const
{
	FDateTime Ignored;
	return ToDateTime(Ignored);
}

int32 FWeatherDateTime::GetDayOfYear() const
{
	FDateTime DateTime;
	return ToDateTime(DateTime) ? DateTime.GetDayOfYear() : 0;
}

double FWeatherDateTime::GetNormalizedDayFraction() const
{
	const double Seconds = static_cast<double>(Hour * 3600 + Minute * 60 + Second);
	return Seconds / FTimespan::FromDays(1.0).GetTotalSeconds();
}

FString FWeatherDateTime::ToDisplayString(const bool bIncludeSeconds) const
{
	return bIncludeSeconds
		? FString::Printf(TEXT("%02d:%02d:%02d"), Hour, Minute, Second)
		: FString::Printf(TEXT("%02d:%02d"), Hour, Minute);
}

bool FWeatherDateTime::operator==(const FWeatherDateTime& Other) const
{
	return Year == Other.Year
		&& Month == Other.Month
		&& Day == Other.Day
		&& Hour == Other.Hour
		&& Minute == Other.Minute
		&& Second == Other.Second;
}
