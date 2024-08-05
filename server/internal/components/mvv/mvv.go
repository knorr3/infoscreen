package mvv

import (
	"encoding/json"
	"fmt"
	"sort"
	"time"

	util "github.com/knorr3/infoscreen/server/internal/util"
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

func GetData(limit int) (departures []Departure, err error) {
	departures = make([]Departure, limit)

	stationId, err := util.GetEnv("MVV_STATION", "")
	if err != nil {
		return
	}
	url := fmt.Sprintf("https://www.mvg.de/api/fib/v2/departure?globalId=%s", stationId)

	body, err := util.MakeAPIRequest(url, "")
	if err != nil {
		return
	}

	allDepartures := make([]Departure, 30)
	jsonErr := json.Unmarshal(body, &allDepartures)
	if jsonErr != nil {
		return
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
