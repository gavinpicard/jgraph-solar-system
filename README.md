# Solar System Visualizer

An overhead view of the solar system (or a planet's moons) rendered with Jgraph Planet and moon positions are fetched in real-time from NASA's [JPL Horizons API](https://ssd.jpl.nasa.gov/horizons/), so every image reflects actual celestial positions.

Orbits are drawn at evenly-spaced radii for visual clarity. Only the
angular positions are astronomically accurate.

## Examples

| File | Description |
|------|-------------|
| `solar_system.jpg` | Static snapshot of all 8 planets |
| `jupiter_moons.jpg` | Io, Europa, Ganymede, Callisto around Jupiter |
| `saturn_moons.jpg` | 7 major moons around Saturn |
| `birthday.jpg` | Solar system on my birthday |
| `earth_moon.jpg` | Earth's sole moon |
| `solar_system_anim.gif` | 1-year animation of the solar system (52 frames) |
| `jupiter_moons_anim.gif` | 18-day animation of Jupiter's Galilean moons |

## Dependencies

- **C compiler** (gcc or clang)
- **Jgraph** — must be compiled in same directory (`./jgraph`)
- **curl** — for fetching positions from the Horizons API
- **ImageMagick** (`convert` / `magick`) — for EPS-to-JPG/GIF conversion

## Build & Run

```bash
make              # compile the program
make images       # compile + generate 5 images
make gifs         # compile + generate 2 gifs
make clean        # remove binary and all generated files
```

### Manual usage

```
./solar_system [-a] [-m planet] [-o outfile] [date]

  -a          Produce an animated GIF instead of a static JPG
  -m planet   Show moons of a planet instead of the solar system
              (earth, mars, jupiter, saturn, uranus, neptune)
  -o file     Output filename (default: solar_system.jpg or .gif)
  date        YYYY-MM-DD (default: today)
```

Examples:

```bash
./solar_system                          # Solar system today
./solar_system 2024-07-04               # Solar system on July 4, 2024
./solar_system -m jupiter               # Jupiter's moons today
./solar_system -a -o orbit.gif          # Animated solar system
./solar_system -a -m saturn -o sat.gif  # Animated Saturn moons
```

## How it works

1. The C program calls the NASA JPL Horizons API via `curl` to fetch
   heliocentric (or planet-centric) X/Y positions in AU.
2. It writes a `.jgr` file where each planet/moon is placed on an
   evenly-spaced circular orbit at its true angular position.
3. Jgraph converts the `.jgr` to PostScript.
4. ImageMagick converts PostScript to JPG (static) or assembles
   individual GIF frames into an animated GIF.

For animations, all timesteps are fetched in a single batch API call
per body, then each frame is rendered individually. Motion trails
(fading dots at previous positions) give a sense of orbital direction
and speed.

## Supported moon systems

| Planet | Moons | Animation |
|--------|-------|-----------|
| Earth | Moon | 30 days, daily frames |
| Mars | Phobos, Deimos | 2 days, 2-hour frames |
| Jupiter | Io, Europa, Ganymede, Callisto | 18 days, 12-hour frames |
| Saturn | Mimas, Enceladus, Tethys, Dione, Rhea, Titan, Iapetus | 18 days, 12-hour frames |
| Uranus | Miranda, Ariel, Umbriel, Titania, Oberon | 14 days, 12-hour frames |
| Neptune | Triton | 7 days, 6-hour frames |
