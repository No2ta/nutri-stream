# nutri-stream
hyddroponics control system for small hydroponics greenhouse
the board used is "https://www.ram-e-shop.com/shop/kit-esp8266-uno-wifi-wifi-arduino-uno-atmega328p-r3-with-esp8266-9286?srsltid=AfmBOoq7w2zBC7M_0psFzmCoB78n5FhEcovu-_HROPQkxviaa21vSZlG"
its a combination of 2 boards an arduino and an esp
so there are dips as you can see in the picture just first use the esp ddip switches (5,6)
to set up the wifi and etc, then use dip switches (3,4) to use the aruino itself.
then finally use dip switches (1,2) now you can not upload anythign but you let the aruino and the esp talk to each other.
the api is grapped from thingspeak.com the write data is used in the proposal.ino
so there are feilds and channel, each channel got its ID so i think its possible to get data from various channels at the same time or not. anyways you use the channel ID as its yous, and then you can see Write and read API, the write API is used in the arduino code, basically its for giving data for thingspeak, so you actually have something to show we can say it like an excell sheet that collects data and each type of data you put go to specefic feild as you put in the code.
The Read is from its name you know, it reads the data so if you want to display it on a website or something you can se the read data to read the saved data in things speak, and the read is used in the script.js, 
thats basically it just make sure to change the write file into your actually write api, the feild with your feild ID, the wifi SSID and password. and make sure to also change the READ api of things speak or you are gonna basically show my data
