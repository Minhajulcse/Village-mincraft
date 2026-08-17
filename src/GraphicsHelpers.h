/* ==========================================================================
 *  GraphicsHelpers.h
 *  --------------------------------------------------------------------------
 *  Low level voxel drawing toolkit for the "Rural Market" 3D scene.
 *
 *  Everything in the scene is assembled out of the primitives declared here,
 *  which keeps the whole project in a clean simplified Minecraft / voxel look
 *  without needing a single shader file.
 *
 *  EP1 (Fundamental Graphics Algorithms) lives here:
 *      -> drawLineBresenham3D()   : custom integer Bresenham line algorithm
 *      -> drawCircleMidpoint3D()  : custom integer Midpoint Circle Algorithm
 *      -> drawDiscMidpoint3D()    : filled variant (span based)
 *      -> drawVoxelSphere()       : voxel rasterised sphere
 * ==========================================================================*/
#ifndef GRAPHICS_HELPERS_H
#define GRAPHICS_HELPERS_H

/* --------------------------------------------------------------------------
 * Platform GL includes.  freeglut ships GL/glut.h on every platform we target
 * (Linux / MSYS2 / Windows freeglut SDK), macOS keeps GLUT in a framework.
 * ------------------------------------------------------------------------ */
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#  include <GLUT/glut.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#  include <GL/glut.h>
#endif

#include <vector>

namespace gh {

/* ==========================================================================
 *  Small maths / colour types
 * ==========================================================================*/

struct Vec3
{
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}
};

struct Color
{
    float r, g, b;

    Color() : r(1.0f), g(1.0f), b(1.0f) {}
    Color(float pr, float pg, float pb) : r(pr), g(pg), b(pb) {}
};

/* Linear interpolation + colour tint helpers. */
float lerp(float a, float b, float t);
Color shade(const Color& c, float factor);          /* multiply, clamped   */
Color mixColor(const Color& a, const Color& b, float t);

/* Deterministic-enough pseudo random helpers (std::rand based). */
float randRange(float lo, float hi);
int   randInt(int lo, int hi);                      /* inclusive           */

/* Distance helpers used by the customer state machine. */
float distXZ(const Vec3& a, const Vec3& b);
float headingXZ(const Vec3& from, const Vec3& to);  /* degrees, +Z == 0    */

/* ==========================================================================
 *  Global render style toggles (driven from main.cpp keyboard callbacks)
 * ==========================================================================*/

void  setOutlineEnabled(bool on);
bool  outlineEnabled();

/* ==========================================================================
 *  Global sunlight state - the day / night cycle
 *  --------------------------------------------------------------------------
 *  The project deliberately uses no GL lights: every face colour is emitted by
 *  hand.  The tint below stands in for sunlight - it multiplies every colour
 *  emitted by the primitives in this file, so dropping it at dusk darkens and
 *  cools the entire world in one place instead of per object.
 *
 *  The sky gradient is the one exception: Scene hands drawVerticalGradient()
 *  colours it has already chosen for the hour, so that call stays untinted.
 *
 *  nightFactor() is the same clock expressed as "how dark is it" (0 in broad
 *  daylight, 1 at midnight); the stalls, houses and carriages read it to know
 *  when to light their lamps.
 * ==========================================================================*/

void  setSunlight(const Color& tint, float night);
const Color& lightTint();
float nightFactor();

/* c * lightTint(), clamped - what every primitive here emits. */
Color tinted(const Color& c);

/* glColor3f(tinted(c)) - for the few call sites that colour their own
 * glBegin/glEnd blocks (tree canopies, hedge spheres, stall produce). */
void  applyColor(const Color& c);

/* --------------------------------------------------------------------------
 *  Lamp light: anything drawn between these two calls ignores the sunlight
 *  tint, so a lit window or a hanging lantern still reads as a glowing source
 *  at midnight instead of being dimmed along with everything around it.
 *  Not nestable - one level is all the scene needs.
 * ------------------------------------------------------------------------ */
void  beginLampLight();
void  endLampLight();

/* ==========================================================================
 *  Face shaded voxel primitives
 *  --------------------------------------------------------------------------
 *  Manual per-face colour differentiation gives the voxel depth cue:
 *      top    -> lightest      front -> base
 *      right  -> darker        left  -> darker still
 *      back   -> dark          bottom-> darkest
 * ==========================================================================*/

/* Cuboid centred on the current matrix origin. */
void drawBlock(float sx, float sy, float sz, float r, float g, float b);
void drawBlock(float sx, float sy, float sz, const Color& c);

/* Cuboid centred at (x,y,z) in the current frame. */
void draw3DCuboid(float x, float y, float z,
                  float sx, float sy, float sz,
                  float r, float g, float b);

/* Triangular prism used for every thatched / gabled roof in the scene.
 * width = X span, height = ridge height, depth = Z span. */
void drawRoofPrism(float x, float y, float z,
                   float width, float height, float depth,
                   const Color& slope, const Color& gable);

/* ==========================================================================
 *  EP1 : Fundamental graphics algorithms
 * ==========================================================================*/

/* Plane selector for the circle rasterisers. */
enum CirclePlane
{
    PLANE_XY = 0,   /* circle stands up, faces the camera */
    PLANE_XZ = 1,   /* circle lies flat on the ground     */
    PLANE_YZ = 2    /* circle stands up, faces sideways   */
};

/* --------------------------------------------------------------------------
 *  CUSTOM BRESENHAM LINE ALGORITHM (integer error terms, no floating point)
 *  Walks from voxel (x0,y0,z0) to (x1,y1,z1) along the dominant axis and
 *  emits one shaded cube per rasterised point, `voxel` world units wide.
 *  Coordinates are voxel indices relative to the world-space origin
 *  (ox,oy,oz).  Used for the straight sun rays.
 * ------------------------------------------------------------------------ */
void drawLineBresenham3D(float ox, float oy, float oz,
                         int x0, int y0, int z0,
                         int x1, int y1, int z1,
                         float voxel, const Color& c);

/* --------------------------------------------------------------------------
 *  CUSTOM MIDPOINT CIRCLE ALGORITHM (integer decision parameter d = 1 - r)
 *  Rasterises a circle of `radius` *voxels* and emits one shaded cube per
 *  rasterised pixel, scaled by `voxel` world units.  Used for the sun core,
 *  voxelised foliage rings, cart wheels and the tea stall smoke rings.
 * ------------------------------------------------------------------------ */
void drawCircleMidpoint3D(float cx, float cy, float cz,
                          int radius, float voxel,
                          CirclePlane plane, const Color& c);

/* Filled companion: the same midpoint decision loop, but each octant pair is
 * closed into a horizontal span of voxels (used for the solid sun disc and
 * the flat voxel cloud / foliage slabs). */
void drawDiscMidpoint3D(float cx, float cy, float cz,
                        int radius, float voxel,
                        CirclePlane plane, const Color& c);

/* Voxel rasterised sphere (shell only - interior cubes are never visible).
 * `radius` is measured in voxels, `voxel` is the world size of one cube. */
void drawVoxelSphere(float cx, float cy, float cz,
                     int radius, float voxel, const Color& c);

/* ==========================================================================
 *  2D overlay / background helpers
 * ==========================================================================*/

/* Switch to a pixel aligned orthographic projection and back. */
void beginOverlay(int screenW, int screenH);
void endOverlay();

/* Full screen vertical gradient (the sky).  Depth writes are disabled. */
void drawVerticalGradient(int screenW, int screenH,
                          const Color& top, const Color& bottom);

} /* namespace gh */

#endif /* GRAPHICS_HELPERS_H */
