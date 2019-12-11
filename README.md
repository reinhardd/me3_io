 
** max2mqtt **

Provides an EQ3 Max Cube heating control system via MQTT

Exposed topics:

  global:
      max2mqtt/<cube-serial>/rooms                          # comma separated list of rooms

  per room:
      max2mqtt/<cube-serial>/<room-name>/act_temp           # float temp in °C
      max2mqtt/<cube-serial>/<room-name>/set_temp           # float temp in °C
      max2mqtt/<cube-serial>/<room-name>/valve_pos          # unsigned 0 .. 100 in %
      max2mqtt/<cube-serial>/<room-name>/mode               # "AUTO"|"MANUAL"|"BOOST"|"VACATION"
      max2mqtt/<cube-serial>/<room-name>/weekplan           # json string TBD


subscrition topics:

  per room
      max2mqtt/<cube-serial>/<room-name>/set/temp           # float in °C
      max2mqtt/<cube-serial>/<room-name>/set/mode           # mode as string "AUTO" | "BOOST" | "MANUAL" | "VACATION ISO_DATE"
      max2mqtt/<cube-serial>/<room-name>/set/weekplan       # json string TBD





