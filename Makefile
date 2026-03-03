CC      = gcc
CFLAGS  = -Wall -O2
LDFLAGS = -lm
JGRAPH  = ./jgraph

IMAGES  = solar_system.jpg         \
          jupiter_moons.jpg        \
          saturn_moons.jpg         \
          birthday.jpg            \
          earth_moon.jpg

GIFS    = solar_system_anim.gif    \
          jupiter_moons_anim.gif

all: solar_system

images: solar_system $(IMAGES)

solar_system: solar_system.c
	$(CC) $(CFLAGS) -DJGRAPH_PATH=\"$(JGRAPH)\" -o $@ $< $(LDFLAGS)

# --- Static snapshots (today's date) ---

solar_system.jpg: solar_system
	./solar_system -o $@

jupiter_moons.jpg: solar_system
	./solar_system -m jupiter -o $@

saturn_moons.jpg: solar_system
	./solar_system -m saturn -o $@

earth_moon.jpg: solar_system
	./solar_system -m earth -o $@

birthday.jpg: solar_system
	./solar_system -d 2004-04-15 -o $@


# --- Animations ---

solar_system_anim.gif: solar_system
	./solar_system -a -o $@

jupiter_moons_anim.gif: solar_system
	./solar_system -a -m jupiter -o $@

clean:
	rm -f solar_system $(IMAGES) *.jgr *.eps
	rm -rf anim_frames

.PHONY: all images clean
