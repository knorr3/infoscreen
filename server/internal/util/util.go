package components

import (
	"log"
	"os"
)

func Getenv(key, fallback string) string {
	value := os.Getenv(key)
	if len(value) == 0 {
		if fallback == "" {
			log.Fatalf("ENV VAR \"%s\" is required but not set!", key)
		}
		return fallback
	}
	return value
}
