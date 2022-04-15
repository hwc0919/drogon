# Redis example

A simple redis chat server

## Usage

First of all you need redis running on the port 6379


## Subscribe and Publish

Go to a websocket test website, such as https://wstool.js.org

### Subscribe

Connect to ws://localhost:8080/sub

To subscribe to a channel, send channel name: `my channel`

To unsubscribe from a channel, send 'unsub ' + channel name: `unsub mychannel`

### Publish

Connect to ws://localhost:8080/pub

To publish message, send channel name and message: `mychannel anything follows will be the message.`
