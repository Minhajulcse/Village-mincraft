/* ==========================================================================
 *  Carriage.h
 *  --------------------------------------------------------------------------
 *  A horse-drawn market cart, and the one-lane road track it travels.
 *
 *  The track is the centre line of the dirt road, re-sampled into a dense
 *  polyline with a cumulative arc length so a carriage can sit anywhere on
 *  the road, not just at the original control nodes.
 *
 *  The road carries TWO-WAY traffic.  Both lanes share one arc-length track
 *  and are separated by offsetting each cart sideways from the centre line:
 *  lane +1 travels with increasing s and keeps to the left of its own
 *  direction, lane -1 travels with decreasing s and does the same, which puts
 *  them on opposite sides of the road.  Oncoming carts therefore pass without
 *  ever being tested against each other - the lane offset alone guarantees
 *  their bodies cannot meet.
 *
 *  Within a lane nobody overtakes, so "how far along is everyone" is the only
 *  state the avoidance needs: each cart reads the arc position of the cart in
 *  front of it IN ITS OWN LANE, keeps a safe following gap, and can never
 *  move further than the gap that exists.  Overlap is impossible by
 *  construction, not by luck - the update clamps the step before it happens.
 *
 *  A cart halts once per lap in the market (random spot, random 4-7 s) to
 *  load and unload goods; everything behind it queues.  Finished carts park
 *  off camera and only re-enter when the spawn point is clear, so respawning
 *  can never teleport one onto the back of another.
 * ==========================================================================*/
#ifndef CARRIAGE_H
#define CARRIAGE_H

#include "GraphicsHelpers.h"

#include <vector>

/* --------------------------------------------------------------------------
 *  RoadTrack : the sampled centre line + arc-length lookups
 * ------------------------------------------------------------------------ */
class RoadTrack
{
public:
    RoadTrack() : mLength(0.0f) {}

    /* Feed it the road centre line and widths Scene already sampled for the
     * dirt, so the lanes can never be wider than the road that is drawn. */
    void build(const std::vector<gh::Vec3>& centreLine,
               const std::vector<float>&    widths);

    float totalLength() const { return mLength; }

    /* World position / heading (degrees about +Y) at arc length s. */
    gh::Vec3 positionAt(float s) const;
    float    headingAt(float s)  const;

    /* Unit normal pointing to the LEFT of increasing s, in the XZ plane.
     * Lane centres are the centre line offset along this. */
    gh::Vec3 normalAt(float s) const;

    /* Half the drawn dirt width at s - the budget the two lanes share. */
    float halfWidthAt(float s) const;

    /* Distance from the centre line to a lane centre at s.  Clamped so two
     * opposing carts can never be closer than their own body width, and so
     * neither hangs off the dirt while the road is wide enough to avoid it. */
    float laneOffsetAt(float s) const;

private:
    std::vector<gh::Vec3> mPts;
    std::vector<float>    mSegLen;   /* per segment, for the arc lookup      */
    std::vector<float>    mHalfW;    /* per sample, half the dirt width      */
    float mLength;
};

/* --------------------------------------------------------------------------
 *  Carriage : one horse, one cart, one driver, one gap rule
 * ------------------------------------------------------------------------ */
class Carriage
{
public:
    Carriage();

    void init(int id, const RoadTrack& track,
              float speed, float startOffset, int lane);

    /* ----------------------------------------------------------------------
     *  Advance s by however much the gap allows.
     *
     *  `pedGap` is the clear run in front of this cart before it reaches
     *  somebody on foot, measured by Scene, which is the only thing that knows
     *  where the villagers are.  It feeds the SAME ramp and the same hard step
     *  clamp as the gap to the cart ahead, so a driver slows for a person in
     *  the road exactly as it slows for a vehicle - and, like the vehicle
     *  case, running one over is impossible by construction rather than
     *  unlikely.  Defaults to "the road is clear" so the existing carts-only
     *  test harnesses still compile.
     * -------------------------------------------------------------------- */
    void update(float dt, const std::vector<Carriage>& all,
                float pedGap = 1.0e9f);

    /* How far ahead of its arc position a cart reaches - the horse's muzzle.
     * Scene needs it to convert a distance-to-person into a usable gap. */
    static float noseAhead();

    /* Re-enter at the start of the road.  Scene owns the decision: it calls
     * this only once no active cart is inside entryClearance() of s = 0. */
    void respawn();

    void draw() const;

    bool  active() const { return mActive; }
    float s()      const { return mS;      }
    int   lane()   const { return mLane;   }

    /* Distance travelled along this cart's own lane, always increasing no
     * matter which way it drives.  Scene uses it for the entry check. */
    float progress() const;

    /* Where the cart body actually is, and which way it points.  Derived from
     * the track and the lane offset exactly as draw() does it, so the gap
     * Scene measures against the pedestrians is measured against the geometry
     * that gets drawn rather than against the bare centre line. */
    gh::Vec3 worldPos() const;
    gh::Vec3 forwardDir() const;

    /* Clear run a parked cart needs at the road entry before re-entering. */
    static float entryClearance();

private:
    void  setProgress(float p);
    void  chooseNextHalt();
    float gapToCarAhead(const std::vector<Carriage>& all) const;
    void  drawHorse() const;
    void  drawCart()  const;

    int              mId;
    const RoadTrack* mTrack;
    float            mS;         /* arc length along the track              */
    float            mSpeed;     /* cruise speed when the road is clear      */
    int              mLane;      /* +1 left, -1 right (lane of the road)    */
    bool             mActive;    /* false == parked off camera, awaiting
                                    a clear spawn point                     */

    float            mSpeedNow;  /* current velocity (unit/s)               */

    /* ---- the market halt ---------------------------------------------- */
    float            mHaltS;     /* arc position of this lap's stop, -1 done */
    float            mHaltTimer; /* counts DOWN; the cart rolls again at 0   */

    /* ---- animation ---------------------------------------------------- */
    float            mWheelSpin; /* degrees, geared to distance covered      */
    float            mGaitPhase; /* leg swing phase for the trot             */
    float            mTailPhase;
};

#endif /* CARRIAGE_H */
