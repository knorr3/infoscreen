package main

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"

	mqtt "github.com/knorr3/infoscreen/server/internal/mqtt"
	util "github.com/knorr3/infoscreen/server/internal/util"
)

func main() {
	// Basic Setup
	var broker = util.Getenv("BROKER_IP", "")
	var port = util.Getenv("BROKER_PORT", "1883")
	var username = util.Getenv("BROKER_USERNAME", "")
	var password = util.Getenv("BROKER_USERNAME", "")

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
