package calendar

import (
	"bytes"
	"sort"
	"time"

	"github.com/apognu/gocal"
	util "github.com/knorr3/infoscreen/server/internal/util"
)

type Event struct {
	Date   time.Time `json:"date"`
	Title  string    `json:"title"`
	AllDay bool      `json:"allDay"`
	Today  bool      `json:"today"`
}

var url string

func New() (err error) {
	url, err = util.GetEnv("ICAL_URL", "")
	return
}

// TODO Timezone handling
func GetData(limit int) (events []Event, err error) {
	body, err := util.MakeAPIRequest(url, "")
	if err != nil {
		return
	}

	// Get the current date to set the "today" field in the response
	now := time.Now()
	today := time.Date(now.Year(), now.Month(), now.Day(), 0, 0, 0, 0, time.Local)

	ics := bytes.NewReader(body)

	// Two weeks
	start, end := time.Now(), time.Now().Add(2*7*24*time.Hour)

	c := gocal.NewParser(ics)
	c.Start, c.End = &start, &end
	err = c.Parse()
	if err != nil {
		return
	}

	// Sort the events slice by datetime
	sort.Slice(c.Events, func(i, j int) bool {
		return c.Events[i].Start.Unix() < c.Events[j].Start.Unix()
	})

	for idx, event := range c.Events {
		if idx >= limit {
			return
		}

		events = append(events, Event{
			Date:  *event.Start,
			Title: event.Summary,
			// AllDay is true if event lasts from 0 to 23:59
			AllDay: event.Start.Hour() == 0 && event.Start.Minute() == 0 && event.End.Hour() == 23 && event.End.Minute() == 59,
			Today:  event.Start.Format("2006-01-02") == today.Format("2006-01-02"),
		})
	}

	return
}
