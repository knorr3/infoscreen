package mqtt

import (
	"encoding/json"
	"fmt"
	"log"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	mvv "github.com/knorr3/infoscreen/server/internal/components/mvv"
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

var baseTopic string = "infoscreen"
var questionTopic string = fmt.Sprintf("%s/question", baseTopic)
var answerTopic string = fmt.Sprintf("%s/answer", baseTopic)

var messagePubHandler mqtt.MessageHandler = func(client mqtt.Client, msg mqtt.Message) {
	// fmt.Printf("Published message: %s from topic: %s\n", msg.Payload(), msg.Topic())
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
	entries := []Entry{}

	station := util.Getenv("MVV_STATION", "none")

	if station != "none" {
		departures, err := mvv.GetData(station, 3) //TODO
		status := "ok"
		if err != nil {
			status = "error"
			log.Fatalf("Failed to get departures: %s", err)
		}

		entries = append(entries, Entry{
			Type:   "db",
			Status: status,
			Data:   departures,
		})
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

func CreateClient(broker string, port string, username string, password string) mqtt.Client {
	opts := mqtt.
		NewClientOptions().
		AddBroker(fmt.Sprintf("tcp://%s:%s", broker, port)).
		SetClientID("go_mqtt_client").
		SetUsername(username).
		SetPassword(password).
		SetDefaultPublishHandler(messagePubHandler).
		SetOnConnectHandler(connectHandler).
		SetConnectionLostHandler(connectLostHandler)

	// Connection
	client := mqtt.NewClient(opts)
	if token := client.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	return client
}
