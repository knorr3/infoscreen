package calendar

import (
	"bytes"
	"sort"
	"time"

	"github.com/apognu/gocal"
	util "github.com/knorr3/infoscreen/server/internal/util"
)

type Event struct {
	Date  time.Time `json:"date"`
	Title string    `json:"title"`
}

func GetData(limit int) (events []Event, err error) {
	url, err := util.GetEnv("ICAL_URL", "")
	if err != nil {
		return
	}

	body, err := util.MakeAPIRequest(url, "")
	if err != nil {
		return
	}

	ics := bytes.NewReader(body)

	// Two weeks
	start, end := time.Now(), time.Now().Add(2*7*24*time.Hour)

	c := gocal.NewParser(ics)
	c.Start, c.End = &start, &end
	err = c.Parse()
	if err != nil {
		return
	}

	for idx, event := range c.Events {
		if idx >= limit {
			return
		}

		events = append(events, Event{
			Date:  *event.Start,
			Title: event.Summary,
		})
	}

	// Sort the events slice by datetime
	sort.Slice(events, func(i, j int) bool {
		return events[i].Date.Unix() < events[j].Date.Unix()
	})

	return
}
