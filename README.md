# maxcube2mqtt

last modified: 13. Dec. 2019

Provides access to an EQ3 Max Cube heating control system via MQTT

**!!! This is work in process, don't expect anything to work. !!!**

** Motivation **

Provide an interface to integrate to EQ3 max cube with node-red.

** Priciples **

The basic element for max cube heating control is the room. So we expose some
topics for every room under control of the cube.

No configuration interface is implemented by now.

maxcube2mqtt is designed to talk exactly to one cube. Without specifying a cube by his serial-no
maxcube2mqtt connects to the first cube that responds to the UDP multicast to port 23272.
Use the -s parameter to select a specific the cube.

** Credits **

https://github.com/Bouni/max-cube-protocol

https://github.com/redboltz/mqtt_cpp

## MQTT - Interface

### exposed topics

  **global**

      max2mqtt/<cube-serial>/rooms                          # comma separated list of rooms

  **per room**

      max2mqtt/<cube-serial>/<room-name>/act-temp           # float temp in °C
      max2mqtt/<cube-serial>/<room-name>/set-temp           # float temp in °C
      max2mqtt/<cube-serial>/<room-name>/valve-pos          # unsigned 0 .. 100 in %
      max2mqtt/<cube-serial>/<room-name>/mode               # "AUTO"|"MANUAL"|"BOOST"|"VACATION"
      max2mqtt/<cube-serial>/<room-name>/weekplan           # weekplan json object


### subscription topics

  **per room**

      max2mqtt/<cube-serial>/<room-name>/set/temp           # float in °C
      max2mqtt/<cube-serial>/<room-name>/set/mode           # mode as string
      max2mqtt/<cube-serial>/<room-name>/set/weekplan       # weekplan json object


  *mode*

  Mode will be set by a simple string

    mode ::= "AUTO" | "BOOST" | "MANUAL" | "VACATION <ISO_DATE>"

  *weekplan json object*

    {
        room: "room_name",
        <dayname> : [
            {
                endtime : "minutes_since_midnight",
                temp :    "<temperature>"
            },
            {
                endtime : "minutes_since_midnight",
                temp :    "<temperature>"
            },
            ....
        ]
    }

    dayname ::= Saturday | Sunday | Monday | Tuesday | Wednesday | Thursday | Friday
    temperature ::= float value i.e.: 17.5

 There is at least one day entry required

### examples

  tbd: add some mosquitto_pub/sub examples


## Build Instructions

prerequisites:

  needed tools: make cmake git c++ (capable of c++17)

  needed libs: boost-dev libssl-dev


    git clone https://github.com/reinhardd/maxcube2mqtt.git
    cd maxcube2mqtt
    git submodule init
    git submodule update
    make -j 4

build is done as an out-of-tree build within .native subdirectory.
to run maxcube2mqtt run

## Run

    .native/maxcube2mqtt -m my_mqtt_broker

You find yourself in a simple command shell that is regularly updated with the actual data from max cube.
Type help list the available commands.

Currently maxcube2mqtt logs to the file xout.log inside the directory where it is run from.


### listen to

