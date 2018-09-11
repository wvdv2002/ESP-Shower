# ESP-Shower
This is the overview of the setup and the code for a water recycling shower using a Sonoff Dual with some hacks.

## Hardware
The hardware consists of an assembly made in cooperation with Delft University.
An overview of the hardware can be found in the [showerdiagram](showerdiagram.pdf).

All the water consumed while showering is catched in the bottom and flows into a buffer tank by gravity.
The pump sucks water through a sieve and pressurizes the system. The water flows through one large particle filter and two (parallel) small particle filter.
After the filter the pressure sensor is mounted. A hose connects to the top side of the system, where a 20L black jerry can functions as a small solar heater and a pressure vessel.
The water flows further through a UV desinfection light and a flow meter. A tap is used to start the flow of water through the shower head. 
After rinsing off the body of the experimental subject the water starts it journey anew.

The controller consists of a Sonoff dual which contains a ESP8266 microcontroller and two relays to control the pump and the UV light.

### Sonoff dual hacks
GPIO0 pin is connected with a diode to the button for easy flashing
GPIO14 and GPIO4 are broken out as explained [here](https://github.com/arendst/Sonoff-Tasmota/issues/797) 
and connected to a pressure switch and a YF-S201C flowmeter.
The flowmeter is powered from the voltage before the voltage regulator, which seems stable enough.


## Software
Using the excellent [ESPUI](https://github.com/s00500/ESPUI) a simple GUI was created to control the pump and the UV light manually.
In the GUI an overview of the following things can be found:
* Total amount of water flowed through the system.
* Actual Flow/s
* Status of the control state machine
* Status of the pump
* Status of the UV light.

### Control explanation
[This flowdiagram](codeflowdiagram.pdf) explains the control part of the code.
The most important part of the controller is to switch on the UV light as soon as water flow is detected (the tap is opened).
This is to make sure that the desinfection works as it should.
If the pressure in the system drops below the threshold (manually set with a set screw in the pressure sensor), the pump will top up the pressure.
If flow is detected the UV light is switched on and the pump started.
