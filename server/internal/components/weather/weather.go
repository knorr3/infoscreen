package weather

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"regexp"
	"time"

	util "github.com/knorr3/infoscreen/server/internal/util"
)

type WeatherResponse struct {
	Version       string    `json:"version"`
	User          string    `json:"user"`
	DateGenerated time.Time `json:"dateGenerated"`
	Status        string    `json:"status"`
	Data          []struct {
		Parameter   string `json:"parameter"`
		Coordinates []struct {
			Lat   float64 `json:"lat"`
			Lon   float64 `json:"lon"`
			Dates []struct {
				Date  time.Time `json:"date"`
				Value float64   `json:"value"`
			} `json:"dates"`
		} `json:"coordinates"`
	} `json:"data"`
}

type Weather struct {
	Date    time.Time `json:"date"`
	Temp    float64   `json:"temp"`
	Precip  float64   `json:"precip"`
	IconIdx int       `json:"icon-idx"`
}

var auth string
var lat string
var long string

// Save the timestamp and data of the last weather request
var weatherTimestamp time.Time
var savedWeather []Weather

func isValidCoordinate(coord string) bool {
	// Define the regular expression pattern for latitude/longitude in "11.111111" format
	pattern := `^-?\d{1,2}\.\d{4,8}$`

	// Compile the regular expression
	re := regexp.MustCompile(pattern)

	// Check if the coordinate matches the pattern
	return re.MatchString(coord)
}

func New() (err error) {
	lat, err = util.GetEnv("WEATHER_LAT", "")
	if err != nil {
		return
	}
	long, err = util.GetEnv("WEATHER_LONG", "")
	if err != nil {
		return
	}
	if !isValidCoordinate(lat) {
		err = fmt.Errorf("latitude not in format \"12.345678\" (1-2 numbers \"dot\" 4-8 numbers): %s", lat)
		return
	}
	if !isValidCoordinate(long) {
		err = fmt.Errorf("longitude not in format \"12.345678\" (1-2 numbers \"dot\" 4-8 numbers): %s", long)
		return
	}

	username, err := util.GetEnv("WEATHER_USERNAME", "")
	if err != nil {
		return
	}
	password, err := util.GetEnv("WEATHER_PASSWORD", "")
	if err != nil {
		return
	}

	auth = base64.StdEncoding.EncodeToString([]byte(username + ":" + password))

	return nil
}

func GetData(limit int) (weather []Weather, err error) {
	// If the weather data is older than 60 minutes, fetch new data
	if time.Since(weatherTimestamp) < 60*time.Minute {
		fmt.Printf("Using saved weather data from %s\n", weatherTimestamp)
		weather = savedWeather
		return
	}

	weather = make([]Weather, limit)
	date := time.Now()

	// TODO honor the limit parameter
	url := fmt.Sprintf("https://api.meteomatics.com/%sT07:00:00Z--%sT17:00:00Z:PT5H/t_2m:C,weather_symbol_1h:idx/%s,%s/json", date.Format("2006-01-02"), date.Format("2006-01-02"), lat, long)

	body, err := util.MakeAPIRequest(url, auth)
	if err != nil {
		return
	}

	weatherResponse := WeatherResponse{}
	err = json.Unmarshal(body, &weatherResponse)

	if weatherResponse.Status != "OK" {
		err = fmt.Errorf("weather API status not ok: %s", weatherResponse.Status)
		return
	}

	// TODO honor the limit parameter
	url = fmt.Sprintf("https://api.meteomatics.com/%sT07:00:00Z--%sT19:00:00Z:PT1H/precip_1h:mm/%s,%s/json", date.Format("2006-01-02"), date.Format("2006-01-02"), lat, long)

	body, err = util.MakeAPIRequest(url, auth)
	if err != nil {
		return
	}

	percipResponse := WeatherResponse{}
	err = json.Unmarshal(body, &percipResponse)

	if percipResponse.Status != "OK" {
		err = fmt.Errorf("weather API status not ok (percip): %s", percipResponse.Status)
		return
	}

	for _, entry := range weatherResponse.Data {
		data := entry.Coordinates[0]

		// TODO honor the limit parameter
		switch entry.Parameter {
		case "t_2m:C":
			weather[0].Temp = data.Dates[0].Value
			weather[1].Temp = data.Dates[1].Value
			weather[2].Temp = data.Dates[2].Value
		// case "precip_1h:mm":
		// 	weather[0].Precip = data.Dates[0].Value
		// 	weather[1].Precip = data.Dates[1].Value
		// 	weather[2].Precip = data.Dates[2].Value
		case "weather_symbol_1h:idx":
			weather[0].IconIdx = int(data.Dates[0].Value)
			weather[1].IconIdx = int(data.Dates[1].Value)
			weather[2].IconIdx = int(data.Dates[2].Value)
		}
	}

	// 7 8 9 10 | 11 12 13 14 | 15 16 17 18 19
	data := percipResponse.Data[0].Coordinates[0]

	// TODO honor the limit parameter
	weather[0].Precip = data.Dates[0].Value
	weather[0].Precip += data.Dates[1].Value
	weather[0].Precip += data.Dates[2].Value
	weather[0].Precip += data.Dates[3].Value
	weather[1].Precip = data.Dates[4].Value
	weather[1].Precip += data.Dates[5].Value
	weather[1].Precip += data.Dates[6].Value
	weather[1].Precip += data.Dates[7].Value
	weather[2].Precip = data.Dates[8].Value
	weather[2].Precip += data.Dates[9].Value
	weather[2].Precip += data.Dates[10].Value
	weather[2].Precip += data.Dates[11].Value
	weather[2].Precip += data.Dates[12].Value

	// Save the current weather data
	savedWeather = weather

	// Save the timestamp of the weather data
	weatherTimestamp = time.Now()

	return
}
