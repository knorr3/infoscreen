package main

import (
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"

	mqtt "github.com/knorr3/infoscreen/server/internal/mqtt"
	util "github.com/knorr3/infoscreen/server/internal/util"
)

func main() {
	// Basic Setup
	broker, err := util.GetEnv("BROKER_IP", "")
	if err != nil {
		log.Fatal(err)
	}
	port, err := util.GetEnv("BROKER_PORT", "1883")
	if err != nil {
		log.Fatal(err)
	}
	username, err := util.GetEnv("BROKER_USERNAME", "")
	if err != nil {
		log.Fatal(err)
	}
	password, err := util.GetEnv("BROKER_USERNAME", "")
	if err != nil {
		log.Fatal(err)
	}

	client := mqtt.CreateClient(broker, port, username, password)

	defer mqtt.DisconnectFromMqtt(client)

	// Setup subscriber
	mqtt.SubscribeToTopic(client)

	// Run infinit
	exit := make(chan os.Signal, 1)
	signal.Notify(exit, syscall.SIGINT, syscall.SIGTERM)
	var sig = <-exit

	fmt.Printf("\nReceived SIG %s\n", sig.String()) //Info
}
