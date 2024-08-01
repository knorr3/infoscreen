package mvv

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"sort"
	"time"
)

type Departure struct {
	PlannedDepartureTime  int64  `json:"plannedDepartureTime"`
	PlannedDepartureTimeS string `json:"plannedDepartureTimeS"`
	// Realtime              bool     `json:"realtime"`
	DelayInMinutes         int    `json:"delayInMinutes"`
	RealtimeDepartureTime  int64  `json:"realtimeDepartureTime"`
	RealtimeDepartureTimeS string `json:"realtimeDepartureTimeS"`
	// TransportType         string   `json:"transportType"`
	Label string `json:"label"`
	// DivaId                string   `json:"divaId"`
	// Network               string   `json:"network"`
	// TrainType             string   `json:"trainType"`
	Destination string `json:"destination"`
	Cancelled   bool   `json:"cancelled"`
	// Sev                   bool     `json:"sev"`
	// Platform              int      `json:"platform"`
	// PlatformChanged       bool     `json:"platformChanged"`
	// Messages              []string `json:"messages"`
	// BannerHash            string   `json:"bannerHash"`
	// Occupancy             string   `json:"occupancy"`
	// StopPointGlobalId     string   `json:"stopPointGlobalId"`
}

func GetData(station string, limit int) (departures []Departure, err error) {
	departures = make([]Departure, limit)
	stationId := station //TODO fetch StationID by name
	url := fmt.Sprintf("https://www.mvg.de/api/fib/v2/departure?globalId=%s", stationId)

	client := http.Client{
		Timeout: time.Second * 2, // Timeout after 2 seconds
	}

	req, err := http.NewRequest(http.MethodGet, url, nil)
	if err != nil {
		log.Fatal("Couldn't create request")
		log.Fatal(err)
		return
	}

	res, err := client.Do(req)
	if err != nil {
		log.Fatal("Couldn't do request")
		log.Fatal(err)
		return
	}

	if res.Body != nil {
		defer res.Body.Close()
	}

	body, err := io.ReadAll(res.Body)
	if err != nil {
		log.Fatal("Couldn't read body")
		log.Fatal(err)
	}

	allDepartures := make([]Departure, 30)
	jsonErr := json.Unmarshal(body, &allDepartures)
	if jsonErr != nil {
		log.Fatal("Couldn't unmarshal body")
		log.Fatal(jsonErr)
	}

	// Sort the trainInfos slice by PlannedDepartureTime
	sort.Slice(allDepartures, func(i, j int) bool {
		return allDepartures[i].PlannedDepartureTime < allDepartures[j].PlannedDepartureTime
	})

	// Filter how many items should be returned
	for i := range limit {
		departures[i] = allDepartures[i]
	}

	// Calculate timestamp from unixtime
	for i, departure := range departures {
		departures[i].PlannedDepartureTimeS = time.Unix(departure.PlannedDepartureTime/1000, 0).Format("15:04")
		departures[i].RealtimeDepartureTimeS = time.Unix(departure.RealtimeDepartureTime/1000, 0).Format("15:04")
	}

	return
}
