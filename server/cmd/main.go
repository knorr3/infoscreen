package main

import (
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"

	mqtt "github.com/knorr3/infoscreen/server/internal/mqtt"
)

func main() {

	client, err := mqtt.New()
	if err != nil {
		log.Fatalf("Error creating MQTT client: %s", err)
	}

	defer mqtt.DisconnectFromMqtt(client)

	// Setup subscriber
	mqtt.SubscribeToTopic(client)

	// Run infinit
	exit := make(chan os.Signal, 1)
	signal.Notify(exit, syscall.SIGINT, syscall.SIGTERM)
	var sig = <-exit

	fmt.Printf("\nReceived SIG %s\n", sig.String()) //Info
}
