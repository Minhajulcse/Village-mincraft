# Rural Market

A simplified Minecraft-style **voxel 3D rendering of a rural village market**, recreated
from the two reference images in `reference/` (`colored.jpg` and `black_and_white.jpg`).
Everything is drawn with the **OpenGL fixed-function pipeline** plus **GLUT/freeglut** —
there are no vertex or fragment shaders anywhere in the project. Depth is instead conveyed
by **manual per-face shading**: each of the six faces of every voxel is emitted with its own
hand-tinted colour (top brightest, front/back mid, sides darker, bottom darkest), which gives
the blocky world its characteristic readable form. On top of the static village the scene
animates wandering customers, gesturing shopkeepers, drifting chimney smoke, circling birds
and a slowly shifting sky, all driven from a single ~60 FPS timer loop.

The village runs on a clock, and everything in it keeps out of everything else:

* **Villagers walk beside the road and never through a stall.** Every customer waypoint is
  offset perpendicular to the road centre line onto the shoulder, on whichever side their own
  stall stands. Past the bend — where the gap between the dirt and stall 2 narrows to about
  1.3 units — the market-side lane leaves the verge and follows the **shop fronts** instead.
  Every leg of every route is tested against every stall footprint, and detoured if it would
  clip one, so nobody ghosts through a canopy. A separation pass keeps the crowd from walking
  through itself.
* **Two-way horse-drawn carts.** Six carts, three each way, sharing one road. Carts in the
  same lane **cannot overlap** — each is clamped so it can never advance past the one ahead.
  Opposing carts are held apart by the lane offset, so they pass without ever touching. One
  cart halts in the market each lap to load goods and the rest visibly queue behind it.
* **Five children playing football.** The ball is passed to a randomly chosen *different*
  child each time, so it circulates round the whole group rather than bouncing between a
  fixed pair, and every child turns to track whoever has it.
* **At dusk the children go home.** The game breaks up, they walk a three-leg route around
  the stall row and in through the cottage door, and they come back out at dawn.
* **A goat grazing in the corner.** It crops the grass at the lower right of the frame,
  lifting its head to step to a fresh patch and settling down for the night.
* **A full day/night cycle.** The sun and moon share one arc, the sky runs through night,
  dawn, day and dusk keyframes, stars fade in after sunset, and the stall lanterns, cart
  tail-lamps and cottage windows light up when it gets dark.

---

## Build & Run

### Linux / macOS — CMake (recommended)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/rural_market
```

Debian / Ubuntu dependencies:

```sh
sudo apt install build-essential cmake freeglut3-dev libglu1-mesa-dev
```

### Linux / macOS — plain make (no CMake)

```sh
make && ./rural_market
```

The `Makefile` auto-detects the platform via `uname -s` and picks the right link flags
(`-lglut -lGLU -lGL -lm` on Linux, `-framework OpenGL -framework GLUT` on macOS).
Object files land in `obj/`, keeping the project root free of build artifacts.
Useful extra targets: `make run`, `make clean`.

### Windows — MSYS2 / MinGW-w64

From the *MSYS2 MinGW 64-bit* shell:

```sh
pacman -S mingw-w64-x86_64-freeglut mingw-w64-x86_64-cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/rural_market.exe
```

`winmm` is linked automatically on Windows — it is required for the ambient-audio
`PlaySound` call. `make` also works from the MSYS2 shell and produces `rural_market.exe`.

---

## Controls

The camera is **fixed**: it never orbits and never zooms, so the framing always matches
the reference still in `reference/colored.jpg`.

| Input | Action |
| --- | --- |
| `O` | Toggle voxel outlines (wireframe edges) |
| `P` | Pause / resume all animation |
| `N` | Skip a quarter day — dawn, noon, dusk, night — to preview the cycle |
| `Esc` or `Q` | Quit |

---

## File layout

```
CMakeLists.txt      CMake build (recommended)
Makefile            plain-make fallback; objects go to obj/
src/                all C++ sources and headers
assets/             sound effects (WAV) + their ATTRIBUTION.md
reference/          the two reference stills the scene was modelled from
```

| Module | Role |
| --- | --- |
| `src/GraphicsHelpers.cpp/.h` | Low-level voxel primitives (shaded cuboids, per-face tinting, outlines, roof prisms) and the **custom Midpoint Circle algorithm** used for round ground decals. Also owns the global **sunlight tint** that every primitive multiplies its colours by, and the lamp-light bracket that exempts a light source from it. |
| `src/Customer.cpp/.h` | A single market visitor: the **4-state path state machine** (walking → shopping → exiting → despawn/respawn) plus the shared 6-cuboid humanoid `drawHuman` used for customers and vendors, and its seated variant `drawHumanSeated` for the cart drivers. |
| `src/Stall.cpp/.h` | One parameterised `drawGenericStall` that renders all **six market stalls** — canopy, counter, crates, displayed goods (spheres / cubes / pyramids / tea), the standing shopkeeper and the hurricane lamp that lights after dusk. |
| `src/Carriage.cpp/.h` | The **two-way horse carts** and the arc-length `RoadTrack` they run on: voxel horse, cart, seated driver, midpoint-circle wheels, lane-aware gap following and the clamp that makes overlap impossible. |
| `src/Scene.cpp/.h` | The whole world: terrain, dirt path, huts, trees, fences, bazar props, birds, the **day/night clock** (sky keyframes, sun and moon arc, stars), the **stall-footprint path planner** (frontage lane, detours, no ghosting), the **children's game with the go-home state machine** and the **grazing goat**, plus the **master update & draw** dispatch and the ambient-audio start/stop. |
| `src/main.cpp` | GLUT initialisation, window/display-mode setup, the single fixed camera (no orbit, no zoom), and all keyboard/reshape/display/timer callbacks. |

Runtime assets: the WAVs in `assets/` (see Audio, below). They are optional — the scene
runs, silently, without them.

---

## Audio

The village has two sound layers, both shipped in `assets/`:

| Clip | Role |
| --- | --- |
| `village_ambient.wav` | a 28 s breeze-and-birdsong bed, crossfaded to loop seamlessly, playing continuously under everything |
| `rooster.wav`, `chickens.wav`, `hens.wav` | farmyard one-shots fired at random 9–24 s intervals from the scene update, so the market never sounds like a static tape |

All four are derived from **public-domain** Wikimedia Commons recordings — sources and the
exact `ffmpeg` processing are listed in [`assets/ATTRIBUTION.md`](assets/ATTRIBUTION.md).

**How they are found.** The program probes `assets/<clip>.wav` first, then the bare filename
in the working directory. The plain-make build runs from the repo root, so it picks up
`assets/` with no copy step; CMake mirrors every WAV into `build/assets/` so
`./build/rural_market` resolves them the same way.

**How they are played.**

* **Linux / macOS** — the bed is a detached child (`setsid`) running the platform player in a
  `while :; do … done` shell loop: `paplay`, falling back to `aplay`, or `afplay` on macOS.
  Each one-shot is its own short-lived `fork` + `exec` of the same player. A `SIGCHLD` handler
  reaps finished players with `waitpid(..., WNOHANG)`, so a long session accumulates no
  zombies. The bed's whole process group is killed on *every* exit path: `atexit` covers a
  normal quit, and a `SIGINT` / `SIGTERM` / `SIGHUP` handler covers Ctrl-C, `kill` and a
  closed terminal — cases `atexit` never runs for, which would otherwise leave the detached
  bed looping with no window left to close. The handler re-raises the original signal, so
  the shell still reports the true exit status.
* **Windows** — `PlaySound(..., SND_FILENAME | SND_ASYNC | SND_LOOP)` from `winmm` for the bed.
  Note that `PlaySound` supports only one sound per process, so while the looping bed holds the
  device the one-shots are suppressed on Windows; the ambient layer still plays normally.

Pausing the scene with `P` also pauses the one-shot timer. **Missing files are not an error** —
each layer degrades independently, the program prints one informational line and carries on.

---

## Road traffic, verges and the day/night cycle

### Villagers walk beside the road, and never through a stall

`kRoad[]` in `src/Scene.cpp` is the road **centre line** — the lane the carts drive down, and
a line no pedestrian ever stands on. Every waypoint handed to a `Customer` goes through
`vergeNode()` first, which pushes it sideways along the perpendicular of the direction of
travel by `halfWidth + margin + stagger`, onto the shoulder. The margin is 2.6 units on
purpose: a waypoint is a chord while the road surface curves, so without it the innermost
lane cuts across the dirt on the inside of a bend.

Which shoulder is not arbitrary: `sideOfRoad()` takes the sign of the dot product between the
stall's counter position and the road's left-hand normal, so a shopper is always put on the
same side as the stall they are heading for and never has to cross the carriageway. The
`stagger` term spreads the crowd across five lanes on the shoulder so they do not march in
single file, and four extra walkers cross the whole map without stopping.

**The frontage lane.** Past the bend the road runs *behind* the stall row, and the strip
between the dirt and stall 2 narrows to about 1.3 units — not a footpath. Any lane threaded
through it either clipped the stall or put people in the road. So on the market side
`walkNode()` stops following the verge at that point and follows `kFrontageLane[]` instead —
points laid along the shop fronts, which is where a market crowd actually walks.

**Nothing walks through a canopy.** This is what fixed the ghosting. Every stall is an
oriented box (`kStallSpecs`, shared with `buildStalls()` so the drawn geometry and the tested
geometry can never drift apart). `legClearOfStalls()` answers *exactly* — Liang–Barsky slab
clipping in the stall's own local frame, no sampling, so a clipped corner cannot slip
through. Every leg of every route is built with `pushLeg()`, which inserts a sidestep detour
when the straight line would cut a stall, and shoppers approach the counter head-on from a
stand-off point on the stall's own axis rather than sliding in from the side.

Path following alone still lets two villagers occupy the same spot — they steer for shared
waypoints at different speeds, so a faster walker simply passes through a slower one. A
relaxation pass in `updateScene()` pushes any overlapping pair apart along the line between
them, capped at 0.05 units per frame so nobody is ever catapulted into the road.

> Verified by a harness that links the real `Scene.o` and inspects the waypoint lists
> `buildCustomers()` actually produced — shipped data, not a re-implementation of it. All 14
> customers, 10 of them shopping: **0** waypoints inside a stall, **0** legs crossing one.

### Carts cannot overlap, in either direction

The road carries **two-way traffic**: six carts, three each way. Both lanes share one
arc-length track and are separated by offsetting each cart sideways from the centre line, so
oncoming carts are held apart by the lane geometry itself and are never even compared against
one another. `RoadTrack` re-samples the centre line with a cumulative arc length, and each
`Carriage` holds a scalar `s` plus a lane.

Because a cart in the reverse lane drives against increasing `s`, the whole following rule is
written against **progress** — distance travelled along your own lane, always increasing —
so one direction-agnostic rule covers both lanes.

Within a lane nobody overtakes, and non-overlap is enforced in two layers:

1. **A following ramp.** `gapToCarAhead()` returns the free road to the nearest cart *in the
   same lane* with greater progress, minus both vehicles' overhangs. Target speed ramps
   linearly from full cruise at a 12-unit gap down to a dead stop at 3 units, and braking is
   applied harder than acceleration.
2. **A hard clamp.** Before the step is committed it is clamped to the gap that actually
   exists: `if (advance > gap) advance = gap`. Even if the ramp misjudges the braking
   distance — a frame-rate spike, a cart halting instantly — a cart *physically cannot* move
   past the one in front. Overlap is impossible by construction rather than by tuning.

Lane separation is `max(halfWidth/2, 2.15)`. The widest point of a cart is the wheel rim at
1.70, so two passing carts keep about 0.9 units of daylight; where the road narrows the
minimum wins, because a cart riding the verge merely looks untidy while two carts sharing one
strip of road looks broken.

Respawning is gated per lane: a cart that reaches the end parks off camera, and `Scene` only
re-admits it once no active cart *in its own lane* is within a full car length plus a cruise
gap of the entry, so a fresh cart can never appear on top of another's tail.

Each cart also halts once per lap in the market for 4–7 seconds to load goods, which is what
makes the rule visible — the carts behind slow, close up and queue.

> Verified over ~6.7 simulated minutes with 250 ms frame hitches, driving the real
> `updateScene()`: same-lane tightest gap **+3.08** units, opposing carts' tightest
> separation **+0.90** units measured in world space with the true 1.70 half-width, and
> **0** breaches of either kind across 83 000 close-approach samples.

### The children's game

Five children stand in a ring on the green in front of the market. `pickReceiver()` chooses a
random *different* child for every pass, so possession circulates instead of ping-ponging
between two players, and every child turns to track whoever has the ball. The ring is an
ellipse squashed in Z, sized and placed so all five are inside the fixed camera's frame.

The group is a state machine: `PLAYING → GOING_HOME → INSIDE → RETURNING`. Once
`nightFactor()` passes 0.55 the game breaks up and they walk home — three legs, because the
direct line from the pitch to the cottage door runs through stalls 1 and 6 — and at dawn they
walk back out and resume. While indoors neither the children nor the ball are drawn; the lit
cottage window is the only sign of them.

> Verified over 4 simulated minutes: 123 passes, every child receiving between 18 and 32
> times, all five involved within 11.6 s, **0** self-passes in 20 000 draws, and the
> home-at-dusk / out-at-dawn cycle completing in both directions.

### The goat

A voxel goat grazes at the lower-right of the frame, on a patch chosen to be ~25 units clear
of the road and well away from the stalls, cottages, trees and the children's pitch. It
alternates between `GRAZE` (neck down, chewing bob) and `STEP` (a short walk to a fresh patch
within a small tether radius) on randomised timers, and settles for the night once it is
dark. Its head hangs off its neck, so lowering the neck to the grass carries the muzzle, ears
and horns with it.

### Day and night

One clock, `mTimeOfDay`, runs 0→1 over 150 s (`0.25` sunrise, `0.5` noon, `0.75` sunset).
Everything else is derived from it:

* `daylight()` and `goldenHour()` blend the sky between night / day / dusk keyframes and
  produce the **sunlight tint** — a colour that `gh::setSunlight()` hands to the primitives
  and every voxel is multiplied by. The project uses no GL lights, so this one multiplier is
  what darkens and cools the whole world at once.
* Sun and moon share one arc half a day out of phase, so the sky is never empty. Both are
  drawn with the same EP1 rasterisers (Bresenham rays, midpoint-circle discs); a second
  offset disc in the sky colour bites the crescent out of the moon.
* Stars fade in with the dark and twinkle on individual phases.
* Lamps invert the tint rather than following it: stall hurricane lamps, cart tail-lamps and
  cottage windows are drawn inside a `beginLampLight()` bracket that exempts them, so they
  read as glowing sources at midnight instead of dimming along with everything else.

The static world is cached in a display list with the tint **baked in**, so the list is
rebuilt when the light drifts more than ~1% in any channel — roughly 30 times across a full
day, and never during the still hours of noon or midnight.

> Measured over a full simulated day at 1280×960: 224 fps mean, p99 17.7 ms, worst frame
> 29 ms, no frame over 33 ms. Freezing the clock drops p99 to 4.8 ms, which confirms the
> 30 slow frames are exactly the 30 relight rebuilds — a one-frame hitch each.

---

## Rubric coverage

**EP1 — Primitives, transformations and the animation loop**

* Custom **Midpoint Circle algorithm** implemented from scratch (integer decision variable
  initialised as `d = 1 - r`, eight-way symmetry, plotted point by point rather than handed
  off to `gluDisk`) in `src/GraphicsHelpers.cpp`, together with its filled span-based disc
  variant and the voxel-rasterised sphere shell built on top of it.
* Full **3D transformation** usage — `glTranslatef` / `glRotatef` / `glScalef` — to place and
  size every voxel, stall, hut and tree: `src/GraphicsHelpers.cpp`, `src/Stall.cpp`,
  `src/Scene.cpp`.
* The **~60 FPS animation loop** is a `glutTimerFunc(16, ...)` re-registering timer in
  `src/main.cpp`, which advances the world through `Scene`'s master update each tick.
* The same rasterisers carry the new content: the **cart wheels** are midpoint-circle rims in
  the `PLANE_YZ` plane (`src/Carriage.cpp`), and the **sun and moon** are midpoint discs with
  custom 3D Bresenham rays, arcing across the sky on the day/night clock (`src/Scene.cpp`).

**EP3 — State machines**

* `src/Customer.cpp/.h` implements a four-state lifecycle: `STATE_WALKING` (follows the path
  waypoints toward a chosen stall), `STATE_SHOPPING` (browses at the counter for a randomised
  **2–4 second** dwell), `STATE_EXITING` (walks back off the map edge) and `STATE_DESPAWN`
  (retires and **respawns** as a fresh visitor with a new target stall), so the market keeps a
  steady population. Transitions are ticked from the scene update in `src/Scene.cpp`.
* `src/Carriage.cpp/.h` adds a second machine on the road: **rolling** (speed governed by the
  gap to the cart ahead), **halted** (loading goods at the market for 4–7 s, which makes the
  carts behind queue) and **parked** (finished the road, waiting off camera for a clear entry
  before respawning).
* The **children** in `src/Scene.cpp` run a third: **playing → going home → inside →
  returning**, driven by the same day/night clock, so the game empties out at dusk and starts
  again at dawn. The **goat** runs a fourth, alternating **grazing** and **stepping** to a
  fresh patch on randomised timers.

**EP4 — Hierarchical modelling**

* Limb hierarchies are built with balanced `glPushMatrix` / `glPopMatrix` pairs following the
  **translate → rotate → draw** pivot convention, so an arm rotates about the shoulder and a
  leg about the hip rather than about the world origin. The shared `drawHuman` in
  `src/Customer.cpp` builds the head/torso/arms/legs hierarchy once; customers drive it with
  both arm and leg swing, while `src/Stall.cpp` reuses it for the shopkeepers with a gesturing
  arm and zero leg swing so vendors stand still behind their counters.
* The carts extend the same idea two levels deeper: `drawHumanSeated` re-poses the shared
  human with a **hip → thigh → knee → shin** chain for the driver, and each horse is a
  **barrel → neck → head → muzzle/ears** stack with four legs rotating about their own hips
  in a diagonal trot, all inside the cart's own world transform.
* The goat is the same pattern again — **body → neck → head → muzzle / ears / horns** — so
  pitching the neck down to graze carries the whole head with it, and the children reuse
  `drawHuman` at child scale for both the kick and the walk home.
