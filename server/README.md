# Server

## Deployment

All components are configured via environment variables:

```bash
# MQTT
BROKER_IP=192.168.0.123
BROKER_USERNAME=mqtt
BROKER_PASSWORD=password

# DB
STATION="München%20Ost"
SKIP_DESTINATION="Tutzing,Feldafing,Possenhofen"

# Weather
WEATHER_USERNAME=user
WEATHER_PASSWORD=password
WEATHER_LAT=50.7753
WEATHER_LONG=6.0839

# Calendar
ICAL_URL=https://calendar.google.com/calendar/ical/anyrandomsubdomaingooglecanthinkof.ics
```

### DB

For details and test of station names, see
https://dbf.finalrewind.org/

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

## Example Output

```json
{
    "time": 1722866621,
    "entries": [
        {
            "type": "db",
            "status": "ok",
            "data": [
                {
                    "plannedDepartureTimeS": "16:09",
                    "delayInMinutes": 8,
                    "realtimeDepartureTimeS": "16:17",
                    "label": "S2",
                    "destination": "Petershausen(Obb)",
                    "cancelled": false
                },
                {
                    "plannedDepartureTimeS": "16:11",
                    "delayInMinutes": 0,
                    "realtimeDepartureTimeS": "16:11",
                    "label": "S6",
                    "destination": "Tutzing",
                    "cancelled": false
                },
                {
                    "plannedDepartureTimeS": "16:13",
                    "delayInMinutes": 0,
                    "realtimeDepartureTimeS": "",
                    "label": "S7",
                    "destination": "Wolfratshausen",
                    "cancelled": true
                }
            ]
        },
        {
            "type": "weather",
            "status": "ok",
            "data": [
                {
                    "date": "0001-01-01T00:00:00Z",
                    "temp": 18.3,
                    "precip": 0,
                    "icon-idx": 4
                },
                {
                    "date": "0001-01-01T00:00:00Z",
                    "temp": 22.5,
                    "precip": 0,
                    "icon-idx": 4
                },
                {
                    "date": "0001-01-01T00:00:00Z",
                    "temp": 18.1,
                    "precip": 0,
                    "icon-idx": 1
                }
            ]
        },
        {
            "type": "calendar",
            "status": "ok",
            "data": [
                {
                    "date": "2024-08-06T13:00:00Z",
                    "title": "Family Event"
                },
                {
                    "date": "2024-08-07T04:15:00Z",
                    "title": "Take out the trash"
                },
                {
                    "date": "2024-08-08T09:45:00Z",
                    "title": "Cinema Time"
                }
            ]
        }
    ]
}
```
