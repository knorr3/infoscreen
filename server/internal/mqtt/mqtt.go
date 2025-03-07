package mqtt

import (
	"encoding/json"
	"fmt"
	"log"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/knorr3/infoscreen/server/internal/components/calendar"
	"github.com/knorr3/infoscreen/server/internal/components/db"
	"github.com/knorr3/infoscreen/server/internal/components/weather"
	util "github.com/knorr3/infoscreen/server/internal/util"
)

type Answer struct {
	Time    int64   `json:"time"`
	Entries []Entry `json:"entries"`
}

type Entry struct {
	Type   string      `json:"type"`
	Status string      `json:"status"`
	Data   interface{} `json:"data"`
}

type DataRequest struct {
	Name  string `json:"name"`
	Count int    `json:"count"`
}

type Request struct {
	Type         string        `json:"type"`
	DataRequests []DataRequest `json:"data"`
}

var baseTopic string = "infoscreen"
var questionTopic string = fmt.Sprintf("%s/question", baseTopic)
var answerTopic string = fmt.Sprintf("%s/answer", baseTopic)

// var messagePubHandler mqtt.MessageHandler = func(client mqtt.Client, msg mqtt.Message) {
// 	fmt.Printf("Published message: %s from topic: %s\n", msg.Payload(), msg.Topic())
// }

func New() (client mqtt.Client, err error) {
	broker, err := util.GetEnv("BROKER_IP", "")
	if err != nil {
		return
	}
	port, err := util.GetEnv("BROKER_PORT", "1883")
	if err != nil {
		return
	}
	username, err := util.GetEnv("BROKER_USERNAME", "")
	if err != nil {
		return
	}
	password, err := util.GetEnv("BROKER_USERNAME", "")
	if err != nil {
		return
	}

	opts := mqtt.
		NewClientOptions().
		AddBroker(fmt.Sprintf("tcp://%s:%s", broker, port)).
		SetClientID("go_mqtt_client").
		SetUsername(username).
		SetPassword(password).
		// SetDefaultPublishHandler(messagePubHandler).
		SetOnConnectHandler(connectHandler).
		SetConnectionLostHandler(connectLostHandler)

	// Connection
	client = mqtt.NewClient(opts)
	if token := client.Connect(); token.Wait() && token.Error() != nil {
		return nil, token.Error()
	}

	// Initialize components
	err = weather.New()
	if err != nil {
		return
	}

	err = db.New()
	if err != nil {
		return
	}

	err = calendar.New()
	if err != nil {
		return
	}

	return client, nil
}

func publish(client mqtt.Client, answer Answer) {
	payload, err := json.Marshal(answer)
	if err != nil {
		panic(err)
	}

	prettyJson, err := json.MarshalIndent(answer, "", "    ")
	if err != nil {
		panic(err)
	}

	fmt.Printf("Publishing:\n%s\n", string(prettyJson))
	token := client.Publish(answerTopic, 0, false, payload) //TODO Topic
	token.Wait()
	time.Sleep(time.Second)
}

var messageSubHandler mqtt.MessageHandler = func(client mqtt.Client, msg mqtt.Message) {
	fmt.Printf("Received message: %s from topic: %s\n", msg.Payload(), msg.Topic())

	// Parse request to Request struct
	request := Request{}
	err := json.Unmarshal(msg.Payload(), &request)
	if err != nil {
		log.Printf("Failed to parse request: %s", err)
		return
	}

	entries := []Entry{}

	// Get data for each request
	for _, dataRequest := range request.DataRequests {
		switch dataRequest.Name {
		case "db":
			departures, err := db.GetData(dataRequest.Count)
			if err != nil {
				log.Printf("Failed to get departures: %s", err)
				entries = append(entries, Entry{
					Type:   "db",
					Status: "err",
					Data:   nil,
				})
			} else {
				entries = append(entries, Entry{
					Type:   "db",
					Status: "ok",
					Data:   departures,
				})
			}
		case "weather":
			weather, err := weather.GetData(dataRequest.Count)
			if err != nil {
				log.Printf("Failed to get weather: %s", err)
				entries = append(entries, Entry{
					Type:   "weather",
					Status: "err",
					Data:   nil,
				})
			} else {
				entries = append(entries, Entry{
					Type:   "weather",
					Status: "ok",
					Data:   weather,
				})
			}
		case "calendar":
			events, err := calendar.GetData(dataRequest.Count)
			if err != nil {
				log.Printf("Failed to get events: %s", err)
				entries = append(entries, Entry{
					Type:   "calendar",
					Status: "err",
					Data:   nil,
				})
			} else {
				entries = append(entries, Entry{
					Type:   "calendar",
					Status: "ok",
					Data:   events,
				})
			}
		default:
			log.Printf("Unknown data request: %s", dataRequest.Name)
			entries = append(entries, Entry{
				Type:   dataRequest.Name,
				Status: "err",
				Data:   nil,
			})
		}
	}

	answer := Answer{
		Time:    time.Now().Unix(),
		Entries: entries,
	}

	publish(client, answer)
}

var connectHandler mqtt.OnConnectHandler = func(client mqtt.Client) {
	fmt.Println("Connected to MQTT server")
}

var connectLostHandler mqtt.ConnectionLostHandler = func(client mqtt.Client, err error) {
	fmt.Printf("Connection lost: %v\n", err)
}

func SubscribeToTopic(client mqtt.Client) {
	token := client.Subscribe(questionTopic, 1, messageSubHandler)
	token.Wait()
	fmt.Printf("Subscribed to topic: %s\n", questionTopic)
}

func DisconnectFromMqtt(client mqtt.Client) {
	fmt.Println("Shutting down MQTT connection")
	client.Disconnect(250)
}
