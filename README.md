# cs410assignment3

Justin Hafling, Irene Mitsiades, and Olivia Riley \
Starting the server: \
"make" will build the webserv executable \
To start the server use "./webserv (port number) (optional cache-enabled)". We used 5053 mostly so "./webserb 5053" but other numbers can be used as well. 

To send requests to the server, open up a browser and type in:
http://localhost:(port number used to set up server)/(request) \
Since we used 5053 this was: \
http://localhost:5053/(request) 

The request can take the form of: 
- blank, which would give you a directory listing on the server
- a .html file on the server
- a .jpg or .gif image on the server
- a .py script on the server
- a .c or other extension file that can be read.

The server also sends status codes: 404 for file not found, 501 for request not yet implemented and 200 for successful.

The arduino has a temperature and PIR sensor on it along with an LCD display to display the temperature and an RGB LED for debugging purposes. It can take the temperature of the stove/oven you place it on or be used for checking the temperature of defrosting meat to know when it is done and the PIR sensor detects when a pot is about to overflow. It also has a 5V stepper motor used to touch the oven to remotely turn it on/off. The RGB LED is red when waiting for a request, green when the request has been recieved, and blue when it has been executed. The LCD display displays the temperature when a request comes in to ask for the temperature in case you are at the stove/oven and want to know the temperature. We soldered the sensors onto protoboards so they can be moved around the kitchen without worrying about wires falling out.

External arduino references: \
Stepper motor - https://create.arduino.cc/projecthub/debanshudas23/getting-started-with-stepper-motor-28byj-48-3de8c9 \
RGB LED - https://create.arduino.cc/projecthub/muhammad-aqib/arduino-rgb-led-tutorial-fc003e \
Liquid crystal tutorial - https://www.arduino.cc/en/Tutorial/LibraryExamples/HelloWorld \
PIR sensor: https://learn.adafruit.com/pir-passive-infrared-proximity-motion-sensor/using-a-pir-w-arduino

GNUplot docs: \
PyGnuplot - https://pypi.org/project/PyGnuplot/
Gnuplot - http://www.gnuplot.info/
