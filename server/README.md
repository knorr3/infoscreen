# Server

## Deployment

All components are configured via environment variables:

```bash
# MQTT
BROKER_IP=192.168.0.123
BROKER_USERNAME=mqtt
BROKER_PASSWORD=password

# MVV
MVV_STATION=de:09178:3110

# Weather
WEATHER_USERNAME=user
WEATHER_PASSWORD=password
WEATHER_LAT=50.7753
WEATHER_LONG=6.0839

# Calendar
ICAL_URL=https://calendar.google.com/calendar/ical/anyrandomsubdomaingooglecanthinkof.ics
```

### MVV

For MVV Station IDs, see:
```
internal/components/mvv/README.md
```

### Weather

I am using "Meteomatics Free Weather API".
You must create an account here:
https://www.meteomatics.com/en/weather-api/weather-api-free/

### Calendar

You can use any webserver that is hosting an ics file.
I am using Google Calendar for this. You can configure your iCal export like this:
* Go to https://calendar.google.com/calendar/u/0/r/settings
* Select you calendar on the bottom left
* Scroll down to the bottom until you see `Secret address in iCal format`
* Set this URL in your environment variable

## Kubernetes

1. Edit the secret.yaml
2. Deploy `secret.yaml` and `deployment.yaml`