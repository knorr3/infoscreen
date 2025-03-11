package components

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"time"
)

func GetEnv(key, fallback string) (value string, err error) {
	value = os.Getenv(key)
	if len(value) == 0 {
		if fallback == "" {
			err = fmt.Errorf("ENV VAR \"%s\" not set", key)
			return
		}
		value = fallback
	}
	return
}

func MakeAPIRequest(url string, auth string) (body []byte, err error) {
	client := http.Client{
		Timeout: time.Second * 5,
	}

	req, err := http.NewRequest(http.MethodGet, url, nil)
	if err != nil {
		return
	}

	if auth != "" {
		req.Header.Add("Authorization", "Basic "+auth)
	}

	res, err := client.Do(req)
	if err != nil {
		return
	}

	if res.Body != nil {
		defer res.Body.Close()
	}

	if res.StatusCode != 200 {
		err = fmt.Errorf(res.Status)
		return
	}

	body, err = io.ReadAll(res.Body)
	return
}

func Contains(slice []string, element string) bool {
	for _, e := range slice {
		if e == element {
			return true
		}
	}
	return false
}
