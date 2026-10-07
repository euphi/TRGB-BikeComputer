# T-RGB Bike Computer

A bike computer built on the [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb): a round
480×480 touch display, ESP32-S3 with BLE and WiFi, SD card, and a LiPo charger on board.
Its main job is **navigation on the handlebar**: turn-by-turn instructions from
[OsmAnd](https://osmand.net/), or along your own GPX route, sent by the Android app
[TrailBridge](trailbridge/index.md) over BLE. On a GPX route it also shows the climb
ahead. Besides that it reads standard BLE bike sensors and logs every ride to SD card.

<div class="shots" markdown>
<figure markdown>![Navigation screen](screenshots/nav.png)<figcaption>Navigation: the next turn</figcaption></figure>
<figure markdown>![Main screen](screenshots/main.png)<figcaption>Main screen with the next maneuver at the top</figcaption></figure>
<figure markdown>![Climb screen](screenshots/climb.png)<figcaption>Climb on a GPX route</figcaption></figure>
</div>

Screenshots from the device, taken during a simulated ride from Villach to Bovec. The
texts on the display are German.

## Navigation in two ways

| | OsmAnd | Your own GPX route |
|---|---|---|
| Who computes the route | OsmAnd on the phone | you, beforehand (e.g. with BRouter, Komoot or a recorded track) |
| Turn instructions | from OsmAnd, with street names and lanes | from the GPX file, or derived from the geometry of the track |
| Elevation profile and climbs | no | yes, if the file has elevation data |
| Needs | OsmAnd and TrailBridge | TrailBridge only |

In both cases the phone stays in the pocket: TrailBridge sends the next maneuver to the
bike computer, which shows it in a pill on the main screen and switches to the full
navigation screen shortly before the turn. How to set it up: [TrailBridge](trailbridge/index.md).

{%
  include-markdown "../README.MD"
  start="<!-- features-start -->"
  end="<!-- features-end -->"
%}

## Where to go next

- [Roadmap](ROADMAP.md): how well each feature is tested, known bugs, planned features
- [TrailBridge](trailbridge/index.md): the Android app that brings navigation to the
  bike computer, and its BLE protocol
- [Climbs](CLIMB.md) and [rides and statistics](design/ride-state-machine.md): how the
  device behaves on the road
- [Road quality](ROADQUALITY.md): the special function around the IMU sensor
- [Log format and tools](TOOLS.md), [log service](LOGSERVICE.md): what happens to the
  recorded rides
- [Debugging](DEBUG.md), [simulator](SIMULATOR.md), [pitfalls](PITFALLS.md),
  [design system](design/rim-ridge-design-system.md): for working on the firmware

Source code: [github.com/euphi/TRGB-BikeComputer](https://github.com/euphi/TRGB-BikeComputer).
Contributions are welcome, see the roadmap for where help is wanted.

## Licences and attributions

- [Cycling icons created by Futuer - Flaticon](https://www.flaticon.com/free-icons/cycling)
- [Parked bicycles - icon by Prashanth Rapolu 15](https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7)
