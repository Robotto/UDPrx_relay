# UDPrx_relay
NodeMCU module (ESP8266) based. Recieves packets on UDP to activate a relay. Also hosts a webserver to do the same. 

Connects to wifi (with wifimanager) and advertises itself via mDNS. Waits for UDP packets with specific contents and activates relay (GPIO).

Also runs a webserver on http://ESPrelay.local (mDNS) . Call the unlock function with http://ESPrelay.local/unlock 

This specific version is used to unlock a door, hence the unlock() function.

Sort of a counterpart to this: https://github.com/Robotto/ESPnfc

HW setup:

| RELAY     | NodeMCU | (ESP)    |
|---------|---------|----------|
| ON      | D3      | (GPIO0)  |