/* ==========================================================================
 *  Scene.h
 *  --------------------------------------------------------------------------
 *  High level environment + orchestration.
 *
 *  Owns:  terrain, the winding dirt path, trees, fences, background hedge,
 *         sun, clouds, birds, 6 market stalls (each with its own shopkeeper)
 *         and the walking customers.
 *
 *  Exposes the master updateScene() / drawScene() pair called from main.cpp.
 * ==========================================================================*/
#ifndef SCENE_H
#define SCENE_H

#include "Carriage.h"
#include "Customer.h"
#include "GraphicsHelpers.h"
#include "Stall.h"

#include <vector>

/* --------------------------------------------------------------------------
 *  Animated sky props
 * ------------------------------------------------------------------------ */
struct Cloud
{
    float x, y, z;
    float scale;
    float speed;
    int   shape;      /* 0..2 : which voxel cluster layout to use */
};

struct Bird
{
    float t;          /* parameter along its 3D sine flight curve */
    float speed;
    float baseY;
    float amp;
    float zPos;
    float flap;       /* wing flap phase */
    float scale;
};

/* --------------------------------------------------------------------------
 *  One star in the night sky.  Fixed position, individual twinkle phase; the
 *  field fades in at dusk and out again at dawn.
 * ------------------------------------------------------------------------ */
struct Star
{
    float x, y, z;
    float size;
    float phase;      /* twinkle */
};

/* --------------------------------------------------------------------------
 *  Village audio: a single continuous ambient bed (breeze + birdsong) that
 *  loops for as long as the window is open.  There are no farmyard one-shots -
 *  the scene has no poultry in it, so a rooster or a clucking hen only ever
 *  sounded like it came from somewhere else.
 *
 *  Windows uses the Multimedia API (PlaySound, with SND_LOOP for the bed);
 *  POSIX builds fall back to a detached paplay/aplay/afplay child. The bed's
 *  process group is terminated by sceneShutdownAudio(), reached both on a normal
 *  exit (atexit) and on a fatal signal (SIGINT/SIGTERM/SIGHUP handler), since a
 *  detached bed that outlives the window has no window left to close. A SIGCHLD
 *  handler reaps the player if it ever exits on its own.
 *  A missing WAV is not an error - the scene just runs quieter.
 * ------------------------------------------------------------------------ */
void sceneShutdownAudio();

class Scene
{
public:
    Scene();

    /* Build all geometry, stalls, customer paths; start the ambient audio. */
    void init();

    /* Master update - called from the 16 ms glutTimerFunc loop. */
    void updateScene(float dt);

    /* Master draw - sky, static world, stalls, customers. */
    void drawScene(int screenW, int screenH);

    /* Fixed viewpoint - the camera never orbits or zooms. */
    void applyCamera() const;         /* issues the gluLookAt */

    void togglePause()   { mPaused = !mPaused; }
    bool paused()  const { return mPaused; }

    /* ----------------------------------------------------------------------
     *  Day / night clock.  mTimeOfDay runs 0..1 over one full day:
     *      0.00 midnight   0.25 sunrise   0.50 noon   0.75 sunset
     *  The 'n' key jumps a quarter day so a marker does not have to sit
     *  through a whole cycle to see the night.
     * -------------------------------------------------------------------- */
    void  skipTimeOfDay(float fraction);
    float timeOfDay() const { return mTimeOfDay; }
    const char* timeOfDayName() const;

    void invalidateStaticGeometry() { mStaticListValid = false; }

private:
    /* ---- construction helpers ----------------------------------------- */
    void buildPathCenterline();
    void buildStalls();
    void buildCustomers();
    void buildCarriages();
    void buildProps();

    /* ---- audio --------------------------------------------------------- */
    void initAudio();

    /* ---- day / night ---------------------------------------------------- */
    /* Height of the sun above the horizon, -1 (midnight) .. +1 (noon). */
    float     sunElevation()  const;
    /* 0 in the dark, 1 in full daylight - the master blend for everything. */
    float     daylight()      const;
    /* Peaks while the sun sits on the horizon: the sunrise / sunset warmth. */
    float     goldenHour()    const;
    /* The colour every voxel in the world is multiplied by this frame. */
    gh::Color sunlightTint()  const;
    void      skyColors(gh::Color& top, gh::Color& low) const;
    /* Where the sun / moon sit on their shared arc across the sky. */
    void      celestialPos(bool moon, float& x, float& y) const;

    /* ---- traffic vs people --------------------------------------------- */
    /* The clear run in front of one cart before it would reach somebody on
     * foot - a villager, a shopper, or one of the children.  Fed straight into
     * Carriage::update() as a gap, so the drivers brake for people on exactly
     * the same rule they use for each other.  1e9 means "the road is clear". */
    float pedestrianGap(const Carriage& c) const;

    /* Shove anyone standing where a cart sweeps back onto the verge.  The
     * relaxation pass that keeps villagers out of each other can walk someone
     * sideways into the road; this is the net under it. */
    void  keepCrowdOffRoad();

    /* ---- environment drawing ------------------------------------------ */
    void drawSky(int screenW, int screenH) const;
    void drawTerrain()        const;
    void drawDirtPath()       const;
    void drawTree(float x, float z, float scale, int variant) const;
    void drawFenceRun(const gh::Vec3& a, const gh::Vec3& b, int posts) const;
    void drawBackgroundHedge() const;

    /* The river down the right of the frame: bare earth banks and a flat
     * cartoon-blue channel, stamped along the river spline the same way the
     * dirt road is stamped along its own.  Static, so it lives in the display
     * list; the drifting ripples and the boats do not. */
    void drawRiver()          const;

    /* The timber bridge carrying the road over the water.  Deck, parapets and
     * piers, laid on the run of road samples buildRiver() found to be over the
     * channel, so it always spans the crossing that is actually there. */
    void drawBridge()         const;

    /* Animated: ripple streaks drifting down the current.  The boats are drawn
     * by a free helper in Scene.cpp, the same way the goat and the children's
     * game are, so they need no member here. */
    void drawRiverRipples()   const;

    /* Thatched village house: cream walls, gabled straw roof, door + windows.
     * Authored at big-house size; the cottage beside it is uniformly scaled
     * down.  windows = number of front windows right of the door (1 or 2). */
    void drawHouse(float x, float z, float rotY, float scale,
                   int windows) const;
    void drawGrassTufts()     const;
    void drawSun()            const;
    void drawMoon()           const;
    void drawStars()          const;
    void drawClouds()         const;
    void drawBirds()          const;

    /* Static (non animated) geometry, cached in a display list. */
    void drawStaticWorld()    const;

    /* ---- data ---------------------------------------------------------- */
    std::vector<MarketStall> mStalls;
    std::vector<Customer>    mCustomers;
    std::vector<Carriage>    mCarriages;
    std::vector<Cloud>       mClouds;
    std::vector<Bird>        mBirds;
    std::vector<Star>        mStars;

    /* the road the carts drive down, sampled by arc length */
    RoadTrack                mTrack;

    /* sampled path centre line (also feeds the customer waypoints) */
    std::vector<gh::Vec3>    mPathPts;
    std::vector<float>       mPathWidth;

    /* scattered decoration, generated once so it never flickers */
    std::vector<gh::Vec3>    mTufts;
    std::vector<gh::Vec3>    mPebbles;

    float  mTime;          /* seconds since start */
    float  mSunSpin;       /* legacy sky phase, degrees */
    float  mTimeOfDay;     /* 0..1 over one day - see skipTimeOfDay() */
    bool   mPaused;

    /* The static world bakes the sunlight tint into its display list, so the
     * list has to be rebuilt whenever the light has moved appreciably.  This
     * is the tint it was last built with. */
    mutable gh::Color mBakedTint;

    mutable GLuint mStaticList;
    mutable bool   mStaticListValid;
};

#endif /* SCENE_H */
