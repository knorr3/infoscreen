package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	mvv "github.com/knorr3/infoscreen/server/components/mvv"
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

func getenv(key, fallback string) string {
	value := os.Getenv(key)
	if len(value) == 0 {
		if fallback == "" {
			log.Fatalf("ENV VAR \"%s\" is required but not set!", key)
		}
		return fallback
	}
	return value
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

	station := getenv("MVV_STATION", "none")

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

func subscribeToTopic(client mqtt.Client, topic string) {
	token := client.Subscribe(topic, 1, messageSubHandler)
	token.Wait()
	fmt.Printf("Subscribed to topic: %s\n", topic)
}

func disconnectFromMqtt(client mqtt.Client) {
	fmt.Println("Shutting down MQTT connection")
	client.Disconnect(250)
}

func main() {
	// Basic Setup
	var broker = getenv("BROKER_IP", "")
	var port = getenv("BROKER_PORT", "1883")
	var username = getenv("BROKER_USERNAME", "")
	var password = getenv("BROKER_USERNAME", "")

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
	defer disconnectFromMqtt(client)

	// Setup subscriber
	subscribeToTopic(client, questionTopic)

	// Run infinit
	exit := make(chan os.Signal, 1)
	signal.Notify(exit, syscall.SIGINT, syscall.SIGTERM)
	var sig = <-exit

	fmt.Printf("\nReceived SIG %s\n", sig.String()) //Info
}
