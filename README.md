# mspirit - PulseAudio Visualizer

![img](mspirit.gif)

###### Horrible artifacts/fps are from the GIF, not the program

### Dependencies

> NOTE: Reliant on PulseAudio Sinks, no ALSA support

- `libpulse-dev`
- [raylib](https://github.com/raysan5/raylib)

### Build/Install

- To build: `make`
- To install: `sudo make install`

### Before you start

1. Create a sink to read from: `pactl load-module module-null-sink sink_name=mspirit`

2. Get the name of the sink you want to combine with: `pactl list sinks short`

3. Combine the sinks: `pactl load-module module-combine-sink sink_name=Combined slaves=mspirit,{YOUR_SINK_NAME}`

4. Switch to the new sink: `pactl set-default-sink Combined` (or use `pavucontrol`)

### Usage

`mspirit Combined.monitor`

#### Controls

- `Escape` and `Q` to quit
- Numbers `1`-`3` to switch visualizer mode
- `Scroll Wheel` to change magnitude
- `H` to hide magnitude


### License:

```
This program is free software.
It is licensed under the GNU GPL version 3 or later.
That means you are free to use this program for any purpose;
free to study and modify this program to suit your needs;
and free to share this program or your modifications with anyone.
If you share this program or your modifications
you must grant the recipients the same freedoms.
To be more specific: you must share the source code under the same license.
For details see https://www.gnu.org/licenses/gpl-3.0.html
```
