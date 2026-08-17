/* ==========================================================================
 *  Customer.h
 *  --------------------------------------------------------------------------
 *  EP3 : Scene decomposition & customer state machine.
 *  EP4 : Hierarchical 3D matrix stacks for the blocky avatar's limbs.
 *
 *  A Customer walks a list of 3D waypoints, stops in front of an assigned
 *  market stall to browse, then continues to the exit and respawns.
 * ==========================================================================*/
#ifndef CUSTOMER_H
#define CUSTOMER_H

#include "GraphicsHelpers.h"

#include <vector>

/* --------------------------------------------------------------------------
 *  The four required states.
 * ------------------------------------------------------------------------ */
enum CustomerState
{
    STATE_WALKING = 0,   /* advancing toward the next 3D waypoint       */
    STATE_SHOPPING,      /* paused 2-4 s in front of a stall, browsing  */
    STATE_EXITING,       /* walking the remaining waypoints to the exit */
    STATE_DESPAWN        /* off-screen at path end -> reset to origin   */
};

/* Palette + body proportions for one blocky villager. */
struct CustomerLook
{
    gh::Color shirt;
    gh::Color trousers;
    gh::Color skin;
    gh::Color hair;
    float     scale;     /* 1.0 == adult height (~4.4 world units) */

    CustomerLook()
        : shirt(0.20f, 0.45f, 0.85f)
        , trousers(0.16f, 0.22f, 0.40f)
        , skin(0.95f, 0.78f, 0.62f)
        , hair(0.10f, 0.09f, 0.10f)
        , scale(1.0f)
    {}
};

/* --------------------------------------------------------------------------
 *  EP4 - the one and only human model in the project: six cuboids
 *  (head, torso, 2 arms, 2 legs) assembled with a hierarchical matrix stack.
 *  Drawn standing on y = 0 facing +Z; the caller supplies the world transform.
 *  Shared verbatim by the walking customers and by every stall vendor -
 *  vendors pass legSwing = 0 so they never appear to walk on the spot.
 * ------------------------------------------------------------------------ */
void drawHuman(const CustomerLook& look,
               float armSwing,
               float legSwing,
               float lean);

/* The same six cuboids, posed for a cart driver: sitting on a bench, thighs
 * forward, shins straight down, one arm out front holding the reins.  Used
 * by the horse-drawn carriages; customers and vendors keep using drawHuman.
 * The avatar still stands 4.4 units tall - the pose sits at the hips. */
void drawHumanSeated(const CustomerLook& look, float armSwing);

class Customer
{
public:
    Customer();

    /* ----------------------------------------------------------------------
     *  path            : ordered 3D waypoints, [0] is the spawn point
     *  shopWaypoint    : index of the waypoint that sits in front of a stall
     *  spawnDelay      : seconds to wait before the first walk (staggering)
     * -------------------------------------------------------------------- */
    void init(int id,
              const std::vector<gh::Vec3>& path,
              int shopWaypoint,
              const CustomerLook& look,
              float speed,
              float spawnDelay);

    /* Advance the state machine and the limb-swing animation. */
    void update(float dt);

    /* Hierarchical draw (EP4) - every limb inside push/pop. */
    void draw() const;

    CustomerState state()  const { return mState; }
    const gh::Vec3& pos()  const { return mPos;   }
    bool  visible()        const { return mVisible; }
    /* True while browsing - used by the stalls to have the owner wave. */
    bool  isShopping()     const { return mState == STATE_SHOPPING; }

    /* --------------------------------------------------------------------
     *  Crowd separation.  Villagers are solid: Scene pushes any pair that
     *  has walked into each other apart by a few centimetres per frame.
     *  The push only moves the body - the waypoint the customer is steering
     *  for is untouched, so they drift back onto their line by themselves.
     * ------------------------------------------------------------------ */
    void  nudge(float dx, float dz) { mPos.x += dx; mPos.z += dz; }

private:
    void  advanceAlongPath(float dt);
    void  arriveAtWaypoint();
    void  resetToOrigin();

    /* ---- identity / appearance ---------------------------------------- */
    int          mId;
    CustomerLook mLook;

    /* ---- path following ----------------------------------------------- */
    std::vector<gh::Vec3> mPath;
    int        mTargetIdx;
    int        mShopWaypoint;
    gh::Vec3   mPos;
    float      mHeading;        /* degrees about +Y                       */
    float      mTargetHeading;
    float      mSpeed;          /* world units / second                   */

    /* ---- state machine ------------------------------------------------ */
    CustomerState mState;
    float      mStateTimer;     /* counts up inside the current state     */
    float      mShopDuration;   /* randomised 2.0 - 4.0 s                 */
    float      mSpawnDelay;
    bool       mVisible;

    /* ---- animation ---------------------------------------------------- */
    float      mSwing;          /* limb swing angle, degrees              */
    float      mSwingPhase;
    float      mBob;            /* vertical body bob while walking        */
    float      mBrowseLean;     /* leaning-in angle while shopping        */
    float      mFade;           /* 1 -> 0 shrink used by STATE_DESPAWN    */
};

#endif /* CUSTOMER_H */
