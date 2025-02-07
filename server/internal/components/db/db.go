package db

import (
	"encoding/json"
	"fmt"
	"sort"
	"strings"

	util "github.com/knorr3/infoscreen/server/internal/util"
)

type Departure struct {
	ScheduledArrival   string `json:"scheduledArrival"`
	ScheduledDeparture string `json:"scheduledDeparture"`
	Destination        string `json:"destination"`
	TrainType          string `json:"train"`
	DelayArrival       int    `json:"delayArrival"`
	DelayDeparture     int    `json:"delayDeparture"`
	IsCancelled        int    `json:"isCancelled"`
}

func GetData(limit int) (departures []Departure, err error) {
	station, err := util.GetEnv("STATION", "")
	if err != nil {
		return
	}
	url := fmt.Sprintf("https://dbf.finalrewind.org/%s.json?limit=10", station)

	body, err := util.MakeAPIRequest(url, "")
	if err != nil {
		return
	}

	allDepartures := make([]Departure, 10)
	err = json.Unmarshal([]byte(body), &struct {
		Departures *[]Departure `json:"departures"`
	}{Departures: &allDepartures})
	if err != nil {
		return
	}

	// Filter out trains ending at the station
	filteredDepartures := []Departure{}
	for _, departure := range allDepartures {
		if departure.Destination != station {
			filteredDepartures = append(filteredDepartures, departure)
		}
	}
	allDepartures = filteredDepartures

	// Sort the trainInfos slice by PlannedDepartureTime
	sort.Slice(allDepartures, func(i, j int) bool {
		return allDepartures[i].ScheduledDeparture < allDepartures[j].ScheduledDeparture
	})

	// Filter how many items should be returned
	limit = min(limit, len(allDepartures))
	for i := range limit {
		departures = append(departures, allDepartures[i])
	}

	// Remove spaces in train type string
	for i, departure := range departures {
		departures[i].TrainType = strings.ReplaceAll(departure.TrainType, " ", "")
	}

	if len(departures) == 0 {
		err = fmt.Errorf("no departures found")
	}

	return
}
