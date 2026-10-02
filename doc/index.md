# T-RGB Bike Computer

A bike computer built on the [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb): a round
480×480 touch display, ESP32-S3 with BLE and WiFi, SD card, and a LiPo charger on board.
It reads standard BLE bike sensors, shows turn-by-turn navigation and the climb ahead
from your phone, logs every ride to SD card and rates the road surface with its own
accelerometer.

<div class="shots" markdown>
<figure markdown>![Main screen](screenshots/main.png)<figcaption>Main</figcaption></figure>
<figure markdown>![Navigation screen](screenshots/nav.png)<figcaption>Navigation</figcaption></figure>
<figure markdown>![Climb screen](screenshots/climb.png)<figcaption>Climb</figcaption></figure>
<figure markdown>![Road label screen](screenshots/roadlabels.png)<figcaption>Road labels</figcaption></figure>
<figure markdown>![Settings screen](screenshots/settings.png)<figcaption>Settings</figcaption></figure>
</div>

Screenshots from the device, taken during a simulated ride from Villach to Bovec. The
texts on the display are German.

{%
  include-markdown "../README.MD"
  start="<!-- features-start -->"
  end="<!-- features-end -->"
%}

## Where to go next

- [Roadmap](ROADMAP.md): how well each feature is tested, known bugs, planned features
- [Climbs](CLIMB.md) and [rides and statistics](design/ride-state-machine.md): how the
  device behaves on the road
- [Log format and tools](TOOLS.md), [log service](LOGSERVICE.md): what happens to the
  recorded rides
- [Debugging](DEBUG.md), [simulator](SIMULATOR.md), [pitfalls](PITFALLS.md),
  [design system](design/rim-ridge-design-system.md): for working on the firmware
- [TrailBridge](trailbridge/index.md): the Android companion app and its BLE protocol

Source code: [github.com/euphi/TRGB-BikeComputer](https://github.com/euphi/TRGB-BikeComputer).
Contributions are welcome, see the roadmap for where help is wanted.

## Licences and attributions

- [Cycling icons created by Futuer - Flaticon](https://www.flaticon.com/free-icons/cycling)
- [Parked bicycles - icon by Prashanth Rapolu 15](https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7)
