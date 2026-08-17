/* ==========================================================================
 *  GraphicsHelpers.cpp - voxel primitive implementation
 * ==========================================================================*/
#include "GraphicsHelpers.h"

#include <cmath>
#include <cstdlib>

namespace gh {

/* ==========================================================================
 *  Face shading table
 * ==========================================================================*/
static const float kShadeTop    = 1.20f;
static const float kShadeBottom = 0.45f;
static const float kShadeFront  = 1.00f;
static const float kShadeBack   = 0.70f;
static const float kShadeRight  = 0.85f;
static const float kShadeLeft   = 0.60f;

static bool gOutline = true;

void setOutlineEnabled(bool on) { gOutline = on; }
bool outlineEnabled()           { return gOutline; }

/* ==========================================================================
 *  Sunlight state (see the header for why this replaces GL lighting)
 * ==========================================================================*/
static Color gTint(1.0f, 1.0f, 1.0f);
static float gNight     = 0.0f;
static Color gSavedTint(1.0f, 1.0f, 1.0f);
static bool  gLampLight = false;

static float clamp01(float v);          /* defined with the maths helpers */

void setSunlight(const Color& tint, float night)
{
    /* A lamp bracket left open would otherwise capture the new tint. */
    if (gLampLight) { gSavedTint = tint; }
    else            { gTint      = tint; }
    gNight = clamp01(night);
}

const Color& lightTint() { return gTint; }
float        nightFactor()   { return gNight; }

void beginLampLight()
{
    if (gLampLight) return;
    gLampLight = true;
    gSavedTint = gTint;
    gTint      = Color(1.0f, 1.0f, 1.0f);
}

void endLampLight()
{
    if (!gLampLight) return;
    gLampLight = false;
    gTint      = gSavedTint;
}

/* ==========================================================================
 *  Maths helpers
 * ==========================================================================*/
float lerp(float a, float b, float t) { return a + (b - a) * t; }

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

Color shade(const Color& c, float factor)
{
    return Color(clamp01(c.r * factor),
                 clamp01(c.g * factor),
                 clamp01(c.b * factor));
}

Color mixColor(const Color& a, const Color& b, float t)
{
    return Color(lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t));
}

Color tinted(const Color& c)
{
    return Color(clamp01(c.r * gTint.r),
                 clamp01(c.g * gTint.g),
                 clamp01(c.b * gTint.b));
}

void applyColor(const Color& c)
{
    const Color t = tinted(c);
    glColor3f(t.r, t.g, t.b);
}

float randRange(float lo, float hi)
{
    const float u = static_cast<float>(std::rand()) /
                    static_cast<float>(RAND_MAX);
    return lo + (hi - lo) * u;
}

int randInt(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + std::rand() % (hi - lo + 1);
}

float distXZ(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

float headingXZ(const Vec3& from, const Vec3& to)
{
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    /* atan2(dx,dz) == 0 when facing +Z, which is straight at the camera. */
    return static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);
}

/* ==========================================================================
 *  drawBlock - the single most used routine in the project
 * ==========================================================================*/
void drawBlock(float sx, float sy, float sz, float r, float g, float b)
{
    drawBlock(sx, sy, sz, Color(r, g, b));
}

void drawBlock(float sx, float sy, float sz, const Color& base)
{
    const float hx = sx * 0.5f;
    const float hy = sy * 0.5f;
    const float hz = sz * 0.5f;

    /* Outlined blocks need a polygon offset so the dark edge lines are not
     * z-fighting with the faces they belong to. */
    const bool outline = gOutline && (sx > 0.30f || sy > 0.30f);
    if (outline)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
    }

    glBegin(GL_QUADS);

    /* ---- top (+Y) : lightest ------------------------------------------- */
    {
        const Color c = shade(base, kShadeTop);
        applyColor(c);
        glVertex3f(-hx,  hy, -hz);
        glVertex3f(-hx,  hy,  hz);
        glVertex3f( hx,  hy,  hz);
        glVertex3f( hx,  hy, -hz);
    }
    /* ---- bottom (-Y) : darkest ---------------------------------------- */
    {
        const Color c = shade(base, kShadeBottom);
        applyColor(c);
        glVertex3f(-hx, -hy, -hz);
        glVertex3f( hx, -hy, -hz);
        glVertex3f( hx, -hy,  hz);
        glVertex3f(-hx, -hy,  hz);
    }
    /* ---- front (+Z) : base tone --------------------------------------- */
    {
        const Color c = shade(base, kShadeFront);
        applyColor(c);
        glVertex3f(-hx, -hy,  hz);
        glVertex3f( hx, -hy,  hz);
        glVertex3f( hx,  hy,  hz);
        glVertex3f(-hx,  hy,  hz);
    }
    /* ---- back (-Z) ---------------------------------------------------- */
    {
        const Color c = shade(base, kShadeBack);
        applyColor(c);
        glVertex3f(-hx, -hy, -hz);
        glVertex3f(-hx,  hy, -hz);
        glVertex3f( hx,  hy, -hz);
        glVertex3f( hx, -hy, -hz);
    }
    /* ---- right (+X) --------------------------------------------------- */
    {
        const Color c = shade(base, kShadeRight);
        applyColor(c);
        glVertex3f( hx, -hy, -hz);
        glVertex3f( hx,  hy, -hz);
        glVertex3f( hx,  hy,  hz);
        glVertex3f( hx, -hy,  hz);
    }
    /* ---- left (-X) ---------------------------------------------------- */
    {
        const Color c = shade(base, kShadeLeft);
        applyColor(c);
        glVertex3f(-hx, -hy, -hz);
        glVertex3f(-hx, -hy,  hz);
        glVertex3f(-hx,  hy,  hz);
        glVertex3f(-hx,  hy, -hz);
    }

    glEnd();

    if (outline)
    {
        glDisable(GL_POLYGON_OFFSET_FILL);

        const Color e = shade(base, 0.34f);
        applyColor(e);
        glBegin(GL_LINES);
            /* four verticals */
            glVertex3f(-hx, -hy, -hz); glVertex3f(-hx,  hy, -hz);
            glVertex3f( hx, -hy, -hz); glVertex3f( hx,  hy, -hz);
            glVertex3f( hx, -hy,  hz); glVertex3f( hx,  hy,  hz);
            glVertex3f(-hx, -hy,  hz); glVertex3f(-hx,  hy,  hz);
            /* top ring */
            glVertex3f(-hx,  hy, -hz); glVertex3f( hx,  hy, -hz);
            glVertex3f( hx,  hy, -hz); glVertex3f( hx,  hy,  hz);
            glVertex3f( hx,  hy,  hz); glVertex3f(-hx,  hy,  hz);
            glVertex3f(-hx,  hy,  hz); glVertex3f(-hx,  hy, -hz);
            /* bottom ring */
            glVertex3f(-hx, -hy, -hz); glVertex3f( hx, -hy, -hz);
            glVertex3f( hx, -hy, -hz); glVertex3f( hx, -hy,  hz);
            glVertex3f( hx, -hy,  hz); glVertex3f(-hx, -hy,  hz);
            glVertex3f(-hx, -hy,  hz); glVertex3f(-hx, -hy, -hz);
        glEnd();
    }
}

/* ==========================================================================
 *  Positioned cuboids
 * ==========================================================================*/
void draw3DCuboid(float x, float y, float z,
                  float sx, float sy, float sz,
                  float r, float g, float b)
{
    glPushMatrix();
        glTranslatef(x, y, z);
        drawBlock(sx, sy, sz, Color(r, g, b));
    glPopMatrix();
}

/* ==========================================================================
 *  Prisms
 * ==========================================================================*/
/* --------------------------------------------------------------------------
 *  Triangular roof prism.  Ridge runs along Z, gables face +/-Z.
 * ------------------------------------------------------------------------ */
void drawRoofPrism(float x, float y, float z,
                   float width, float height, float depth,
                   const Color& slope, const Color& gable)
{
    glPushMatrix();
    glTranslatef(x, y, z);

    const float hx = width * 0.5f;
    const float hz = depth * 0.5f;

    /* ---- two sloped planes ------------------------------------------- */
    const Color right = shade(slope, 1.06f);
    const Color left  = shade(slope, 0.74f);

    glBegin(GL_QUADS);
        applyColor(right);
        glVertex3f(0.0f, height, -hz);
        glVertex3f(0.0f, height,  hz);
        glVertex3f(  hx,   0.0f,  hz);
        glVertex3f(  hx,   0.0f, -hz);

        applyColor(left);
        glVertex3f(0.0f, height, -hz);
        glVertex3f( -hx,   0.0f, -hz);
        glVertex3f( -hx,   0.0f,  hz);
        glVertex3f(0.0f, height,  hz);
    glEnd();

    /* ---- gable triangles --------------------------------------------- */
    const Color gFront = shade(gable, 1.00f);
    const Color gBack  = shade(gable, 0.70f);

    glBegin(GL_TRIANGLES);
        applyColor(gFront);
        glVertex3f( -hx, 0.0f,  hz);
        glVertex3f(  hx, 0.0f,  hz);
        glVertex3f(0.0f, height, hz);

        applyColor(gBack);
        glVertex3f(  hx, 0.0f, -hz);
        glVertex3f( -hx, 0.0f, -hz);
        glVertex3f(0.0f, height, -hz);
    glEnd();

    if (gOutline)
    {
        const Color e = shade(slope, 0.36f);
        applyColor(e);
        glBegin(GL_LINES);
            /* ridge */
            glVertex3f(0.0f, height, -hz); glVertex3f(0.0f, height, hz);
            /* front gable outline */
            glVertex3f(-hx, 0.0f, hz);  glVertex3f( hx, 0.0f, hz);
            glVertex3f( hx, 0.0f, hz);  glVertex3f(0.0f, height, hz);
            glVertex3f(0.0f, height, hz); glVertex3f(-hx, 0.0f, hz);
            /* back gable outline */
            glVertex3f(-hx, 0.0f, -hz); glVertex3f( hx, 0.0f, -hz);
            glVertex3f( hx, 0.0f, -hz); glVertex3f(0.0f, height, -hz);
            glVertex3f(0.0f, height, -hz); glVertex3f(-hx, 0.0f, -hz);
            /* eaves */
            glVertex3f(-hx, 0.0f, -hz); glVertex3f(-hx, 0.0f, hz);
            glVertex3f( hx, 0.0f, -hz); glVertex3f( hx, 0.0f, hz);
        glEnd();
    }

    glPopMatrix();
}

/* ==========================================================================
 *  EP1.0 - CUSTOM BRESENHAM LINE ALGORITHM (3D, integer only)
 * --------------------------------------------------------------------------
 *  The 2D algorithm generalised to three axes.  Pick the axis with the
 *  largest absolute delta as the driving axis, step it one voxel at a time,
 *  and carry an integer error term for each of the other two axes:
 *
 *      err = 2 * delta_minor - delta_major
 *      each step:  if (err > 0) { minor++ ; err -= 2 * delta_major }
 *                  err += 2 * delta_minor
 *
 *  No floating point and no division - exactly as the classic formulation.
 *  One shaded voxel cube is emitted per rasterised point.
 * ==========================================================================*/
void drawLineBresenham3D(float ox, float oy, float oz,
                         int x0, int y0, int z0,
                         int x1, int y1, int z1,
                         float voxel, const Color& c)
{
    int x = x0, y = y0, z = z0;

    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int dz = std::abs(z1 - z0);

    const int sx = (x1 >= x0) ? 1 : -1;
    const int sy = (y1 >= y0) ? 1 : -1;
    const int sz = (z1 >= z0) ? 1 : -1;

    /* emit the voxel at the current raster position */
    #define GH_PLOT()                                        \
        do {                                                 \
            glPushMatrix();                                  \
                glTranslatef(ox + x * voxel,                 \
                             oy + y * voxel,                 \
                             oz + z * voxel);                \
                drawBlock(voxel, voxel, voxel, c);           \
            glPopMatrix();                                   \
        } while (0)

    if (dx >= dy && dx >= dz)            /* ---- X is the driving axis ---- */
    {
        int ey = 2 * dy - dx;
        int ez = 2 * dz - dx;

        for (int i = 0; i <= dx; ++i)
        {
            GH_PLOT();
            if (ey > 0) { y += sy; ey -= 2 * dx; }
            if (ez > 0) { z += sz; ez -= 2 * dx; }
            ey += 2 * dy;
            ez += 2 * dz;
            x  += sx;
        }
    }
    else if (dy >= dx && dy >= dz)       /* ---- Y is the driving axis ---- */
    {
        int ex = 2 * dx - dy;
        int ez = 2 * dz - dy;

        for (int i = 0; i <= dy; ++i)
        {
            GH_PLOT();
            if (ex > 0) { x += sx; ex -= 2 * dy; }
            if (ez > 0) { z += sz; ez -= 2 * dy; }
            ex += 2 * dx;
            ez += 2 * dz;
            y  += sy;
        }
    }
    else                                 /* ---- Z is the driving axis ---- */
    {
        int ex = 2 * dx - dz;
        int ey = 2 * dy - dz;

        for (int i = 0; i <= dz; ++i)
        {
            GH_PLOT();
            if (ex > 0) { x += sx; ex -= 2 * dz; }
            if (ey > 0) { y += sy; ey -= 2 * dz; }
            ex += 2 * dx;
            ey += 2 * dy;
            z  += sz;
        }
    }

    #undef GH_PLOT
}

/* ==========================================================================
 *  EP1.1 - CUSTOM MIDPOINT CIRCLE ALGORITHM
 * --------------------------------------------------------------------------
 *  Classic integer formulation:
 *
 *      x = 0 ; y = r ; d = 1 - r
 *      while (x <= y)
 *          plot 8 symmetric points
 *          if (d < 0)  d += 2x + 3            (choose E)
 *          else      { d += 2(x - y) + 5 ; y-- }  (choose SE)
 *          x++
 *
 *  Instead of writing single pixels we emit one shaded voxel cube per
 *  rasterised point, which turns the 2D algorithm into 3D scene geometry.
 * ==========================================================================*/

/* Map a rasterised (u,v) offset onto the requested world plane. */
static void plotVoxel(float cx, float cy, float cz,
                      int u, int v, float voxel,
                      CirclePlane plane, const Color& c)
{
    float ox = 0.0f, oy = 0.0f, oz = 0.0f;

    switch (plane)
    {
        case PLANE_XY: ox = u * voxel; oy = v * voxel;                 break;
        case PLANE_XZ: ox = u * voxel; oz = v * voxel;                 break;
        case PLANE_YZ: oy = u * voxel; oz = v * voxel;                 break;
        default:       ox = u * voxel; oy = v * voxel;                 break;
    }

    glPushMatrix();
        glTranslatef(cx + ox, cy + oy, cz + oz);
        drawBlock(voxel, voxel, voxel, c);
    glPopMatrix();
}

void drawCircleMidpoint3D(float cx, float cy, float cz,
                          int radius, float voxel,
                          CirclePlane plane, const Color& c)
{
    if (radius < 1)
    {
        plotVoxel(cx, cy, cz, 0, 0, voxel, plane, c);
        return;
    }

    int x = 0;
    int y = radius;
    int d = 1 - radius;              /* <-- the decision parameter */

    while (x <= y)
    {
        /* eight-way symmetry of one rasterised octant */
        plotVoxel(cx, cy, cz,  x,  y, voxel, plane, c);
        plotVoxel(cx, cy, cz,  y,  x, voxel, plane, c);
        plotVoxel(cx, cy, cz, -x,  y, voxel, plane, c);
        plotVoxel(cx, cy, cz, -y,  x, voxel, plane, c);
        plotVoxel(cx, cy, cz, -x, -y, voxel, plane, c);
        plotVoxel(cx, cy, cz, -y, -x, voxel, plane, c);
        plotVoxel(cx, cy, cz,  x, -y, voxel, plane, c);
        plotVoxel(cx, cy, cz,  y, -x, voxel, plane, c);

        if (d < 0)
        {
            d += 2 * x + 3;                  /* east */
        }
        else
        {
            d += 2 * (x - y) + 5;            /* south-east */
            --y;
        }
        ++x;
    }
}

void drawDiscMidpoint3D(float cx, float cy, float cz,
                        int radius, float voxel,
                        CirclePlane plane, const Color& c)
{
    if (radius < 1)
    {
        plotVoxel(cx, cy, cz, 0, 0, voxel, plane, c);
        return;
    }

    /* Run the very same midpoint loop, but record the horizontal extent of
     * every scan-line so the circle can be filled with voxel spans.        */
    std::vector<int> extent(static_cast<size_t>(radius) + 1, 0);

    int x = 0;
    int y = radius;
    int d = 1 - radius;

    while (x <= y)
    {
        if (y <= radius && x > extent[static_cast<size_t>(y)])
            extent[static_cast<size_t>(y)] = x;
        if (x <= radius && y > extent[static_cast<size_t>(x)])
            extent[static_cast<size_t>(x)] = y;

        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            --y;
        }
        ++x;
    }

    for (int row = -radius; row <= radius; ++row)
    {
        const int half = extent[static_cast<size_t>(std::abs(row))];
        for (int col = -half; col <= half; ++col)
            plotVoxel(cx, cy, cz, col, row, voxel, plane, c);
    }
}

/* --------------------------------------------------------------------------
 *  Voxel rasterised sphere.  Only the shell is emitted: any voxel whose six
 *  neighbours are all inside the sphere can never be seen.
 * ------------------------------------------------------------------------ */
void drawVoxelSphere(float cx, float cy, float cz,
                     int radius, float voxel, const Color& base)
{
    if (radius < 1) radius = 1;

    const int r2 = radius * radius;

    for (int i = -radius; i <= radius; ++i)
        for (int j = -radius; j <= radius; ++j)
            for (int k = -radius; k <= radius; ++k)
            {
                const int d2 = i * i + j * j + k * k;
                if (d2 > r2) continue;

                /* shell test */
                const bool interior =
                    ((i + 1) * (i + 1) + j * j + k * k <= r2) &&
                    ((i - 1) * (i - 1) + j * j + k * k <= r2) &&
                    (i * i + (j + 1) * (j + 1) + k * k <= r2) &&
                    (i * i + (j - 1) * (j - 1) + k * k <= r2) &&
                    (i * i + j * j + (k + 1) * (k + 1) <= r2) &&
                    (i * i + j * j + (k - 1) * (k - 1) <= r2);
                if (interior) continue;

                /* subtle per-voxel tone variation so the shell is not flat */
                const float f = 0.92f + 0.10f * ((i + j + k) & 1);

                glPushMatrix();
                    glTranslatef(cx + i * voxel,
                                 cy + j * voxel,
                                 cz + k * voxel);
                    drawBlock(voxel, voxel, voxel, shade(base, f));
                glPopMatrix();
            }
}

/* ==========================================================================
 *  2D overlay helpers
 * ==========================================================================*/
void beginOverlay(int screenW, int screenH)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<GLdouble>(screenW),
               0.0, static_cast<GLdouble>(screenH));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
}

void endOverlay()
{
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawVerticalGradient(int screenW, int screenH,
                          const Color& top, const Color& bottom)
{
    beginOverlay(screenW, screenH);

    /* Deliberately NOT tinted: Scene picks the sky colours for the current
     * hour, so running them through the sunlight tint would darken dusk and
     * midnight twice over. */
    glBegin(GL_QUADS);
        glColor3f(bottom.r, bottom.g, bottom.b);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(static_cast<float>(screenW), 0.0f);
        glColor3f(top.r, top.g, top.b);
        glVertex2f(static_cast<float>(screenW), static_cast<float>(screenH));
        glVertex2f(0.0f, static_cast<float>(screenH));
    glEnd();

    endOverlay();
}

} /* namespace gh */
