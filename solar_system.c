/*
 * solar_system.c — Overhead view of the solar system (or a planet's moons)
 *                  rendered via Jgraph, with optional animation.
 *
 * Positions are fetched from the NASA JPL Horizons API.
 * Orbits are drawn at evenly-spaced radii; only the angular
 * positions are accurate.
 *
 * Usage:
 *   ./solar_system [-a] [-m planet] [-o outfile] [date]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>

#ifndef JGRAPH_PATH
#define JGRAPH_PATH "./jgraph"
#endif

#define NUM_PLANETS 8
#define ORBIT_PTS   360
#define MAX_MOONS   16
#define MAX_FRAMES  600
#define MAX_BODIES  16
#define TRAIL_LEN   4

typedef struct {
  const char *name;
  int    id;
  double x, y;
  double r, g, b;
} Body;

typedef struct {
  const char *name;
  int    center_id;
  double r, g, b;
  int    num_moons;
  Body   moons[MAX_MOONS];
  int    anim_days;
  double anim_step_hours;
  const char *anim_step_str;   /* URL-encoded for Horizons (+ = space) */
} MoonSystem;


static Body planets_template[NUM_PLANETS] = {
  {"Mercury", 199, 0,0, 0.75,0.75,0.75},
  {"Venus",   299, 0,0, 0.95,0.80,0.30},
  {"Earth",   399, 0,0, 0.30,0.60,1.00},
  {"Mars",    499, 0,0, 0.90,0.30,0.15},
  {"Jupiter", 599, 0,0, 0.90,0.65,0.30},
  {"Saturn",  699, 0,0, 0.90,0.80,0.50},
  {"Uranus",  799, 0,0, 0.55,0.85,0.95},
  {"Neptune", 899, 0,0, 0.30,0.40,0.90},
};

static const double planet_mark_size[NUM_PLANETS] =
  {0.30, 0.36, 0.40, 0.32, 0.60, 0.55, 0.46, 0.46};

#define NUM_MOON_SYSTEMS 6
static MoonSystem moon_systems[NUM_MOON_SYSTEMS] = {
  {"Earth", 399, 0.30,0.60,1.00, 1, {
    {"Moon", 301, 0,0, 0.82,0.82,0.82},
  }, 30, 24.0, "1+d"},

  {"Mars", 499, 0.90,0.30,0.15, 2, {
    {"Phobos", 401, 0,0, 0.72,0.65,0.55},
    {"Deimos", 402, 0,0, 0.78,0.72,0.62},
  }, 2, 2.0, "2+h"},

  {"Jupiter", 599, 0.90,0.65,0.30, 4, {
    {"Io",       501, 0,0, 0.95,0.85,0.30},
    {"Europa",   502, 0,0, 0.85,0.90,1.00},
    {"Ganymede", 503, 0,0, 0.70,0.65,0.60},
    {"Callisto", 504, 0,0, 0.55,0.50,0.45},
  }, 18, 12.0, "12+h"},

  {"Saturn", 699, 0.90,0.80,0.50, 7, {
    {"Mimas",     601, 0,0, 0.80,0.80,0.80},
    {"Enceladus", 602, 0,0, 0.88,0.93,1.00},
    {"Tethys",    603, 0,0, 0.80,0.80,0.75},
    {"Dione",     604, 0,0, 0.75,0.75,0.70},
    {"Rhea",      605, 0,0, 0.72,0.72,0.68},
    {"Titan",     606, 0,0, 0.90,0.70,0.30},
    {"Iapetus",   608, 0,0, 0.58,0.52,0.46},
  }, 18, 12.0, "12+h"},

  {"Uranus", 799, 0.55,0.85,0.95, 5, {
    {"Miranda", 701, 0,0, 0.72,0.72,0.72},
    {"Ariel",   702, 0,0, 0.85,0.85,0.85},
    {"Umbriel", 703, 0,0, 0.55,0.55,0.58},
    {"Titania", 704, 0,0, 0.80,0.76,0.76},
    {"Oberon",  705, 0,0, 0.65,0.60,0.55},
  }, 14, 12.0, "12+h"},

  {"Neptune", 899, 0.30,0.40,0.90, 1, {
    {"Triton", 801, 0,0, 0.80,0.75,0.85},
  }, 7, 6.0, "6+h"},
};

static char *run_cmd(const char *cmd)
{
  FILE *fp = popen(cmd, "r");
  if (!fp) return NULL;

  size_t cap = 16384, len = 0;
  char *buf = malloc(cap);
  char tmp[1024];
  size_t n;
  while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
    while (len + n + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
    memcpy(buf + len, tmp, n);
    len += n;
  }
  buf[len] = '\0';
  pclose(fp);
  return buf;
}

static void replace_ext(const char *path, const char *ext,
                        char *out, int len)
{
  const char *dot = strrchr(path, '.');
  int base = dot ? (int)(dot - path) : (int)strlen(path);
  snprintf(out, len, "%.*s%s", base, path, ext);
}

static void date_add_days(const char *start, int days, char *out, int len)
{
  struct tm tm = {0};
  sscanf(start, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
  tm.tm_year -= 1900;
  tm.tm_mon  -= 1;
  tm.tm_mday += days;
  mktime(&tm);
  strftime(out, len, "%Y-%m-%d", &tm);
}

static MoonSystem *find_moon_system(const char *name)
{
  for (int i = 0; i < NUM_MOON_SYSTEMS; i++)
    if (strcasecmp(name, moon_systems[i].name) == 0)
      return &moon_systems[i];
  return NULL;
}

static int fetch_positions(int body, int center,
                           const char *start, const char *stop,
                           const char *step,
                           double *xs, double *ys, int max_pts)
{
  char cmd[1024];
  snprintf(cmd, sizeof(cmd),
           "curl -s 'https://ssd.jpl.nasa.gov/api/horizons.api?"
           "format=text"
           "&COMMAND=%%27%d%%27"
           "&OBJ_DATA=%%27NO%%27"
           "&MAKE_EPHEM=%%27YES%%27"
           "&EPHEM_TYPE=%%27VECTORS%%27"
           "&CENTER=%%27500@%d%%27"
           "&START_TIME=%%27%s%%27"
           "&STOP_TIME=%%27%s%%27"
           "&STEP_SIZE=%%27%s%%27"
           "&VEC_TABLE=%%271%%27"
           "&OUT_UNITS=%%27AU-D%%27'",
           body, center, start, stop, step);

  char *resp = run_cmd(cmd);
  if (!resp) return -1;

  char *soe = strstr(resp, "$$SOE");
  char *eoe = strstr(resp, "$$EOE");
  if (!soe || !eoe) { free(resp); return -1; }

  int count = 0;
  char *p = soe;
  while (count < max_pts) {
    p = strstr(p + 1, "X =");
    if (!p || p >= eoe) break;
    double xv, yv;
    if (sscanf(p, "X = %lf Y = %lf", &xv, &yv) == 2 ||
      sscanf(p, "X =%lf Y =%lf", &xv, &yv) == 2) {
      xs[count] = xv;
      ys[count] = yv;
      count++;
    }
  }
  free(resp);
  return count;
}

static int fetch_position(int body, int center,
                          const char *d0, const char *d1,
                          double *ox, double *oy)
{
  double xs[2], ys[2];
  int n = fetch_positions(body, center, d0, d1, "1+d", xs, ys, 2);
  if (n < 1) return -1;
  *ox = xs[0];  *oy = ys[0];
  return 0;
}

static void jgr_graph_header(FILE *fp, double lim, double sz)
{
  fprintf(fp, "newgraph\n");
  fprintf(fp, "xaxis min %.1f max %.1f size %.1f nodraw\n", -lim, lim, sz);
  fprintf(fp, "yaxis min %.1f max %.1f size %.1f nodraw\n", -lim, lim, sz);
  fprintf(fp, "\n");
}

static void jgr_background(FILE *fp, double lim)
{
  fprintf(fp, "newline poly pcfill 0.04 0.04 0.12 "
          "color 0.04 0.04 0.12\n");
  fprintf(fp, "  pts %.1f %.1f  %.1f %.1f  %.1f %.1f  %.1f %.1f\n\n",
          -lim, -lim, lim, -lim, lim, lim, -lim, lim);
}

static void jgr_title(FILE *fp, double y, const char *text)
{
  fprintf(fp, "newstring hjc vjb fontsize 18 font Helvetica-Bold "
          "lcolor 1 1 1\n");
  fprintf(fp, "  x 0 y %.2f : %s\n", y, text);
}

static void jgr_subtitle(FILE *fp, double y, const char *text)
{
  fprintf(fp, "newstring hjc vjt fontsize 10 font Helvetica "
          "lcolor 0.50 0.50 0.60\n");
  fprintf(fp, "  x 0 y %.2f : %s\n\n", y, text);
}

static void jgr_orbit(FILE *fp, double radius,
                      double cr, double cg, double cb)
{
  fprintf(fp, "newcurve marktype none linetype dotted "
          "color %.3f %.3f %.3f\n", cr, cg, cb);
  fprintf(fp, "  pts");
  for (int i = 0; i <= ORBIT_PTS; i++) {
    double a = 2.0 * M_PI * i / ORBIT_PTS;
    if (i % 10 == 0) fprintf(fp, "\n   ");
    fprintf(fp, " %.4f %.4f", radius * cos(a), radius * sin(a));
  }
  fprintf(fp, "\n\n");
}

static void jgr_circle(FILE *fp, double x, double y, double ms,
                       double r, double g, double b)
{
  fprintf(fp, "newcurve marktype circle marksize %.2f %.2f "
          "cfill %.2f %.2f %.2f color %.2f %.2f %.2f\n",
          ms, ms, r, g, b, r, g, b);
  fprintf(fp, "  pts %.4f %.4f\n", x, y);
}

static void jgr_label(FILE *fp, double x, double y, double angle,
                      int fontsize, const char *name)
{
  const char *hj = (cos(angle) >= 0) ? "hjl" : "hjr";
  const char *vj = (sin(angle) >= 0) ? "vjb" : "vjt";
  fprintf(fp, "newstring %s %s fontsize %d font Helvetica "
          "lcolor 0.90 0.90 0.95\n", hj, vj, fontsize);
  fprintf(fp, "  x %.4f y %.4f : %s\n\n", x, y, name);
}

static void jgr_body(FILE *fp, double px, double py, double angle,
                     double ms, double r, double g, double b,
                     double label_off, int fontsize, const char *name)
{
  jgr_circle(fp, px, py, ms, r, g, b);
  double lx = px + label_off * cos(angle);
  double ly = py + label_off * sin(angle);
  jgr_label(fp, lx, ly, angle, fontsize, name);
}

static void jgr_sun(FILE *fp)
{
  jgr_circle(fp, 0, 0, 1.0, 0.18, 0.15, 0.12);
  jgr_circle(fp, 0, 0, 0.55, 1.00, 0.85, 0.10);
  fprintf(fp, "newstring hjc vjt fontsize 10 font Helvetica-Bold "
          "lcolor 1.00 0.85 0.30\n");
  fprintf(fp, "  x 0 y -0.55 : Sun\n\n");
}

static void jgr_center_planet(FILE *fp, const char *name,
                              double r, double g, double b)
{
  jgr_circle(fp, 0, 0, 0.80, r * 0.25, g * 0.25, b * 0.25);
  jgr_circle(fp, 0, 0, 0.45, r, g, b);
  fprintf(fp, "newstring hjc vjt fontsize 10 font Helvetica-Bold "
          "lcolor %.2f %.2f %.2f\n", r, g, b);
  fprintf(fp, "  x 0 y -0.50 : %s\n\n", name);
}

static void jgr_trail(FILE *fp, double orbit_r, double ms,
                      double r, double g, double b,
                      double *xs, double *ys, int frame)
{
  static const double fade[]   = {0.50, 0.30, 0.18, 0.10};
  static const double shrink[] = {0.70, 0.48, 0.30, 0.18};

  for (int t = 1; t <= TRAIL_LEN && frame - t >= 0; t++) {
    double a  = atan2(ys[frame - t], xs[frame - t]);
    double f  = fade[t - 1];
    double sz = ms * shrink[t - 1];
    jgr_circle(fp, orbit_r * cos(a), orbit_r * sin(a), sz,
               r * f, g * f, b * f);
  }
}

static int run_jgraph_and_convert(const char *jgr, const char *eps,
                                  const char *out, int density)
{
  char cmd[512];
  snprintf(cmd, sizeof(cmd), JGRAPH_PATH " %s > %s", jgr, eps);
  if (system(cmd) != 0) return -1;

  snprintf(cmd, sizeof(cmd),
           "convert -density %d %s %s 2>/dev/null", density, eps, out);
  if (system(cmd) != 0) {
    snprintf(cmd, sizeof(cmd),
             "magick -density %d %s %s", density, eps, out);
    if (system(cmd) != 0) return -1;
  }
  return 0;
}

static void usage(const char *prog)
{
  fprintf(stderr,
          "Usage: %s [-a] [-m planet] [-o outfile] [date]\n"
          "  -a          Animate (produce GIF)\n"
          "  -m planet   Show moons of a planet\n"
          "              (earth, mars, jupiter, saturn, uranus, neptune)\n"
          "  -o file     Output filename (default: solar_system.jpg / .gif)\n"
          "  date        YYYY-MM-DD (default: today)\n", prog);
}

int main(int argc, char **argv)
{
  const char *date_arg = NULL;
  const char *moon_arg = NULL;
  const char *out_arg  = NULL;
  int animate = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-m") == 0) {
      if (++i >= argc) { usage(argv[0]); return 1; }
      moon_arg = argv[i];
    } else if (strcmp(argv[i], "-a") == 0) {
      animate = 1;
    } else if (strcmp(argv[i], "-o") == 0) {
      if (++i >= argc) { usage(argv[0]); return 1; }
      out_arg = argv[i];
    } else if (strcmp(argv[i], "-h") == 0) {
      usage(argv[0]); return 0;
    } else if (argv[i][0] != '-') {
      date_arg = argv[i];
    }
  }

  /* Default output filename */
  char outfile[256];
  if (out_arg) {
    snprintf(outfile, sizeof(outfile), "%s", out_arg);
  } else {
    snprintf(outfile, sizeof(outfile), "%s",
             animate ? "solar_system.gif" : "solar_system.jpg");
  }

  /* Today's date as default */
  time_t now = time(NULL);
  struct tm t0 = *gmtime(&now);
  char d0[16], d1[16];
  strftime(d0, sizeof(d0), "%Y-%m-%d", &t0);
  time_t tmrw = now + 86400;
  struct tm t1 = *gmtime(&tmrw);
  strftime(d1, sizeof(d1), "%Y-%m-%d", &t1);
  if (date_arg) { strncpy(d0, date_arg, 15); d0[15] = '\0'; }

  /* Derive intermediate filenames from output */
  char jgr_file[260], eps_file[260];
  replace_ext(outfile, ".jgr", jgr_file, sizeof(jgr_file));
  replace_ext(outfile, ".eps", eps_file, sizeof(eps_file));

  if (animate) {
    static double ax[MAX_BODIES][MAX_FRAMES];
    static double ay[MAX_BODIES][MAX_FRAMES];
    int nframes = 0, nbodies = 0;

    double br[MAX_BODIES], bg[MAX_BODIES], bb[MAX_BODIES];
    double bms[MAX_BODIES];
    const char *bnames[MAX_BODIES];
    int is_moon = 0;
    double lim, step_hours;
    MoonSystem *ms = NULL;

    if (moon_arg) {
      ms = find_moon_system(moon_arg);
      if (!ms) {
        fprintf(stderr, "No moon data for '%s'.\n"
                "Supported: earth, mars, jupiter, saturn, "
                "uranus, neptune\n", moon_arg);
        return 1;
      }
      is_moon = 1;
      nbodies = ms->num_moons;
      lim = nbodies + 1.8;
      step_hours = ms->anim_step_hours;

      char stop[16];
      date_add_days(d0, ms->anim_days, stop, sizeof(stop));
      fprintf(stderr, "Fetching %s moons (%s to %s)...\n",
              ms->name, d0, stop);

      for (int i = 0; i < nbodies; i++) {
        Body *m = &ms->moons[i];
        bnames[i] = m->name;
        br[i] = m->r; bg[i] = m->g; bb[i] = m->b;
        bms[i] = 0.30;
        fprintf(stderr, "  %-10s ... ", m->name); fflush(stderr);
        int n = fetch_positions(m->id, ms->center_id, d0, stop,
                                ms->anim_step_str,
                                ax[i], ay[i], MAX_FRAMES);
        if (n < 1) { fprintf(stderr, "FAILED\n"); return 1; }
        if (i == 0) nframes = n; else if (n < nframes) nframes = n;
        fprintf(stderr, "%d positions\n", n);
      }
    } else {
      nbodies = NUM_PLANETS;
      lim = 9.8;
      step_hours = 168.0;

      char stop[16];
      date_add_days(d0, 365, stop, sizeof(stop));
      fprintf(stderr, "Fetching planets (%s to %s)...\n", d0, stop);

      for (int i = 0; i < nbodies; i++) {
        Body *p = &planets_template[i];
        bnames[i] = p->name;
        br[i] = p->r; bg[i] = p->g; bb[i] = p->b;
        bms[i] = planet_mark_size[i];
        fprintf(stderr, "  %-8s ... ", p->name); fflush(stderr);
        int n = fetch_positions(p->id, 10, d0, stop, "7+d",
                                ax[i], ay[i], MAX_FRAMES);
        if (n < 1) { fprintf(stderr, "FAILED\n"); return 1; }
        if (i == 0) nframes = n; else if (n < nframes) nframes = n;
        fprintf(stderr, "%d positions\n", n);
      }
    }

    /* Generate frames */
    system("mkdir -p anim_frames");
    fprintf(stderr, "Generating %d frames...\n", nframes);

    for (int f = 0; f < nframes; f++) {
      fprintf(stderr, "\r  Frame %d/%d  ", f + 1, nframes);
      fflush(stderr);

      char fj[64], fe[64], fg[64];
      snprintf(fj, 64, "anim_frames/f%04d.jgr", f);
      snprintf(fe, 64, "anim_frames/f%04d.eps", f);
      snprintf(fg, 64, "anim_frames/f%04d.gif", f);

      FILE *fp = fopen(fj, "w");
      if (!fp) continue;

      jgr_graph_header(fp, lim, 7.5);
      jgr_background(fp, lim);

      char title[128];
      if (is_moon) {
        snprintf(title, sizeof(title), "%s - Moons - Day %.1f",
                 ms->name, f * step_hours / 24.0);
      } else {
        char fd[16];
        date_add_days(d0, (int)(f * step_hours / 24.0),
                      fd, sizeof(fd));
        snprintf(title, sizeof(title), "Solar System - %s", fd);
      }
      jgr_title(fp, lim - 1.0, title);

      for (int i = 0; i < nbodies; i++)
        jgr_orbit(fp, i + 1.0,
                  br[i] * 0.30, bg[i] * 0.30, bb[i] * 0.30);

      if (is_moon)
        jgr_center_planet(fp, ms->name, ms->r, ms->g, ms->b);
      else
        jgr_sun(fp);

      for (int i = 0; i < nbodies; i++) {
        double angle = atan2(ay[i][f], ax[i][f]);
        double orbit = i + 1.0;
        double px = orbit * cos(angle);
        double py = orbit * sin(angle);

        jgr_trail(fp, orbit, bms[i], br[i], bg[i], bb[i],
                  ax[i], ay[i], f);
        jgr_body(fp, px, py, angle, bms[i],
                 br[i], bg[i], bb[i],
                 bms[i] * 0.5 + 0.20, 10, bnames[i]);
      }
      fclose(fp);

      run_jgraph_and_convert(fj, fe, fg, 150);
    }
    fprintf(stderr, "\n");

    /* Assemble frames into animated GIF */
    fprintf(stderr, "Assembling GIF...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "convert -delay 8 -loop 0 anim_frames/f*.gif '%s' 2>/dev/null",
             outfile);
    if (system(cmd) != 0) {
      snprintf(cmd, sizeof(cmd),
               "magick -delay 8 -loop 0 anim_frames/f*.gif '%s'", outfile);
      system(cmd);
    }
    system("rm -rf anim_frames");
    fprintf(stderr, "Wrote %s\n", outfile);

  } else {
    FILE *fp;

    if (moon_arg) {
      /* Moon view */
      MoonSystem *ms = find_moon_system(moon_arg);
      if (!ms) {
        fprintf(stderr, "No moon data for '%s'.\n"
                "Supported: earth, mars, jupiter, saturn, "
                "uranus, neptune\n", moon_arg);
        return 1;
      }

      fprintf(stderr, "Fetching %s moons for %s...\n", ms->name, d0);
      for (int i = 0; i < ms->num_moons; i++) {
        Body *m = &ms->moons[i];
        fprintf(stderr, "  %-10s ... ", m->name); fflush(stderr);
        if (fetch_position(m->id, ms->center_id, d0, d1,
                           &m->x, &m->y) != 0) {
          fprintf(stderr, "FAILED\n"); return 1;
        }
        fprintf(stderr, "r=%.6f AU\n",
                sqrt(m->x * m->x + m->y * m->y));
      }

      fp = fopen(jgr_file, "w");
      if (!fp) { perror(jgr_file); return 1; }

      int nm = ms->num_moons;
      double lim = nm + 1.8;
      jgr_graph_header(fp, lim, 7.5);
      jgr_background(fp, lim);

      char title[128];
      snprintf(title, sizeof(title), "%s - Moons - %s", ms->name, d0);
      jgr_title(fp, lim - 0.8, title);

      for (int i = 0; i < nm; i++) {
        Body *m = &ms->moons[i];
        jgr_orbit(fp, i + 1.0,
                  m->r * 0.35, m->g * 0.35, m->b * 0.35);
      }
      jgr_center_planet(fp, ms->name, ms->r, ms->g, ms->b);

      for (int i = 0; i < nm; i++) {
        Body *m = &ms->moons[i];
        double angle = atan2(m->y, m->x);
        double orbit = i + 1.0;
        jgr_body(fp, orbit * cos(angle), orbit * sin(angle),
                 angle, 0.30, m->r, m->g, m->b,
                 0.33, 10, m->name);
      }

    } else {
      /* Solar system view */
      Body planets[NUM_PLANETS];
      memcpy(planets, planets_template, sizeof(planets));

      fprintf(stderr, "Fetching planets for %s...\n", d0);
      for (int i = 0; i < NUM_PLANETS; i++) {
        fprintf(stderr, "  %-8s ... ", planets[i].name);
        fflush(stderr);
        if (fetch_position(planets[i].id, 10, d0, d1,
                           &planets[i].x, &planets[i].y) != 0) {
          fprintf(stderr, "FAILED\n"); return 1;
        }
        fprintf(stderr, "r=%.3f AU\n",
                sqrt(planets[i].x * planets[i].x +
                     planets[i].y * planets[i].y));
      }

      fp = fopen(jgr_file, "w");
      if (!fp) { perror(jgr_file); return 1; }

      double lim = 9.8;
      jgr_graph_header(fp, lim, 7.5);
      jgr_background(fp, lim);

      char title[128];
      snprintf(title, sizeof(title), "Solar System - %s", d0);
      jgr_title(fp, lim - 1.2, title);

      for (int i = 0; i < NUM_PLANETS; i++)
        jgr_orbit(fp, i + 1.0,
                  planets[i].r * 0.30, planets[i].g * 0.30,
                  planets[i].b * 0.30);
      jgr_sun(fp);

      for (int i = 0; i < NUM_PLANETS; i++) {
        Body *p = &planets[i];
        double angle = atan2(p->y, p->x);
        double orbit = i + 1.0;
        jgr_body(fp, orbit * cos(angle), orbit * sin(angle),
                 angle, planet_mark_size[i],
                 p->r, p->g, p->b,
                 planet_mark_size[i] * 0.5 + 0.20, 10, p->name);
      }
    }

    fclose(fp);
    fprintf(stderr, "Wrote %s\n", jgr_file);

    if (run_jgraph_and_convert(jgr_file, eps_file, outfile, 300) != 0) {
      fprintf(stderr, "Conversion failed\n");
      return 1;
    }
    fprintf(stderr, "Wrote %s\n", outfile);
  }

  return 0;
}
