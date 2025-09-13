# timezones.py
import datetime
import pytz

# Current UTC time
now_utc = datetime.datetime.utcnow().replace(tzinfo=pytz.utc)

utc_minus_12 = now_utc.astimezone(pytz.timezone("Etc/GMT+12")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_11 = now_utc.astimezone(pytz.timezone("Pacific/Midway")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_10 = now_utc.astimezone(pytz.timezone("Pacific/Honolulu")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_9  = now_utc.astimezone(pytz.timezone("America/Anchorage")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_8  = now_utc.astimezone(pytz.timezone("America/Los_Angeles")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_7  = now_utc.astimezone(pytz.timezone("America/Denver")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_6  = now_utc.astimezone(pytz.timezone("America/Chicago")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_5  = now_utc.astimezone(pytz.timezone("America/New_York")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_4  = now_utc.astimezone(pytz.timezone("America/Puerto_Rico")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_3  = now_utc.astimezone(pytz.timezone("America/Argentina/Buenos_Aires")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_2  = now_utc.astimezone(pytz.timezone("Atlantic/South_Georgia")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_minus_1  = now_utc.astimezone(pytz.timezone("Atlantic/Azores")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_0   = now_utc.astimezone(pytz.timezone("UTC")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_1   = now_utc.astimezone(pytz.timezone("Europe/Berlin")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_2   = now_utc.astimezone(pytz.timezone("Europe/Kiev")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_3   = now_utc.astimezone(pytz.timezone("Europe/Moscow")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_4   = now_utc.astimezone(pytz.timezone("Asia/Dubai")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_5   = now_utc.astimezone(pytz.timezone("Asia/Karachi")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_6   = now_utc.astimezone(pytz.timezone("Asia/Dhaka")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_7   = now_utc.astimezone(pytz.timezone("Asia/Bangkok")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_8   = now_utc.astimezone(pytz.timezone("Asia/Shanghai")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_9   = now_utc.astimezone(pytz.timezone("Asia/Tokyo")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_10  = now_utc.astimezone(pytz.timezone("Australia/Sydney")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_11  = now_utc.astimezone(pytz.timezone("Pacific/Noumea")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_12  = now_utc.astimezone(pytz.timezone("Pacific/Auckland")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_13  = now_utc.astimezone(pytz.timezone("Pacific/Tongatapu")).strftime("%Y-%m-%d %I:%M:%S %p")
utc_plus_14  = now_utc.astimezone(pytz.timezone("Pacific/Kiritimati")).strftime("%Y-%m-%d %I:%M:%S %p")
