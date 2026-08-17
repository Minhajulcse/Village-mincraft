/* ==========================================================================
 *  Carriage.cpp
 *  --------------------------------------------------------------------------
 *  The horse-drawn carts that use the road, and the arc-length track they
 *  run on.
 *
 *  EP1 : the cart wheels are rims rasterised by the custom Midpoint Circle
 *        Algorithm (gh::drawCircleMidpoint3D) in the YZ plane.
 *  EP3 : each cart is a small state machine - rolling, halted at the market,
 *        finished and parked off camera awaiting a clear spawn point.
 *  EP4 : horse and driver are hierarchical matrix stacks; every leg rotates
 *        about its own hip, every wheel spoke about its own hub.
 * ==========================================================================*/
#include "Carriage.h"
#include "Customer.h"                  /* drawHumanSeated() + CustomerLook */

#include <algorithm>
#include <cmath>

using gh::Color;
using gh::Vec3;

/* ==========================================================================
 *  Dimensions and palette
 * ==========================================================================*/
namespace {

/* ---- footprint, measured from the cart's own origin (facing +Z) --------- *
 * Everything the avoidance rule needs to know about the shape of a cart is
 * these two numbers: how far it reaches in front of its arc position, and
 * how far behind.  kNoseAhead is derived from the horse transform chain
 * below (muzzle lands ~8.6 units out) with a little margin on top - if the
 * horse geometry changes, this has to change with it or carts will appear to
 * overlap even though the arc-length rule says they do not. */
const float kNoseAhead  =  8.9f;    /* horse's muzzle                      */
const float kTailBehind =  4.3f;    /* rear board                          */
const float kCarLen     = kNoseAhead + kTailBehind;

/* ---- following rule ---------------------------------------------------- */
const float kGapStop    =  3.0f;    /* below this the cart is stopped dead  */
const float kGapCruise  = 12.0f;    /* above this it runs at full speed     */
const float kAccel      =  2.6f;    /* unit/s^2, speeding up                */
const float kBrake      =  7.0f;    /* unit/s^2, slowing down (harder)      */

/* Clear run needed at the start of the road before a parked cart re-enters.
 * One car length plus a cruise gap, so a fresh cart never lands on a tail. */
const float kEntryClear = kCarLen + kGapCruise;

/* ---- cart body --------------------------------------------------------- */
const float kWheelVoxR  = 6;        /* wheel radius, in voxels             */
const float kWheelVox   = 0.24f;    /* world size of one rim voxel         */
const float kWheelR     = kWheelVoxR * kWheelVox;
const float kAxleZ      = -2.00f;
const float kBedY       =  2.15f;
const float kBenchTop   =  3.05f;

/* ---- horse ------------------------------------------------------------- */
const float kHorseZ     =  4.60f;   /* barrel centre                       */
const float kLegLen     =  2.35f;
const float kBarrelY    =  3.25f;
const float kNeckLen    =  1.55f;
const float kNeckLean   = 40.0f;    /* degrees forward off vertical        */
const float kMuzzlePitch = 12.0f;   /* head angled this far BELOW level    */

const Color kCartWood  (0.52f, 0.34f, 0.17f);
const Color kCartRail  (0.62f, 0.43f, 0.22f);
const Color kWheelRim  (0.38f, 0.25f, 0.13f);
const Color kSpokeCol  (0.58f, 0.40f, 0.20f);
const Color kIron      (0.26f, 0.25f, 0.27f);
const Color kSackCol   (0.72f, 0.62f, 0.36f);
const Color kHoof      (0.16f, 0.14f, 0.13f);

/* Four coat colours, picked by cart id so the team is never all one horse. */
const Color kCoat[4] =
{
    Color(0.42f, 0.26f, 0.14f),     /* bay        */
    Color(0.20f, 0.17f, 0.16f),     /* near black */
    Color(0.76f, 0.66f, 0.50f),     /* dun        */
    Color(0.55f, 0.36f, 0.20f)      /* chestnut   */
};
const Color kShirtCol[4] =
{
    Color(0.90f, 0.90f, 0.86f),
    Color(0.24f, 0.50f, 0.78f),
    Color(0.82f, 0.44f, 0.16f),
    Color(0.36f, 0.60f, 0.32f)
};

} /* anonymous namespace */

/* ==========================================================================
 *  RoadTrack - dense samples of the road centre line + arc lengths
 * ==========================================================================*/
void RoadTrack::build(const std::vector<Vec3>& centreLine,
                      const std::vector<float>& widths)
{
    mPts.clear();
    mSegLen.clear();
    mHalfW.clear();
    mLength = 0.0f;

    if (centreLine.size() < 2) return;

    mPts = centreLine;

    /* The dirt is drawn from these widths, so taking the lane budget from the
     * same array is what keeps a cart from hanging over the verge. */
    mHalfW.resize(mPts.size(), 4.0f);
    for (std::size_t i = 0; i < mPts.size() && i < widths.size(); ++i)
        mHalfW[i] = widths[i] * 0.5f;

    mSegLen.reserve(mPts.size() - 1);

    for (std::size_t i = 0; i + 1 < mPts.size(); ++i)
    {
        const float len = gh::distXZ(mPts[i], mPts[i + 1]);
        mSegLen.push_back(len);
        mLength += len;
    }
}

Vec3 RoadTrack::positionAt(float s) const
{
    if (mPts.empty())  return Vec3();
    if (mPts.size() == 1 || s <= 0.0f) return mPts.front();
    if (s >= mLength)  return mPts.back();

    float walked = 0.0f;
    for (std::size_t i = 0; i < mSegLen.size(); ++i)
    {
        if (s <= walked + mSegLen[i])
        {
            const float t = (mSegLen[i] > 1.0e-5f)
                          ? (s - walked) / mSegLen[i] : 0.0f;
            const Vec3& a = mPts[i];
            const Vec3& b = mPts[i + 1];
            return Vec3(gh::lerp(a.x, b.x, t), 0.0f, gh::lerp(a.z, b.z, t));
        }
        walked += mSegLen[i];
    }
    return mPts.back();
}

/* Central difference: sampling either side of s rounds the polyline corners
 * off, so a cart leans into a bend instead of snapping round it. */
float RoadTrack::headingAt(float s) const
{
    return gh::headingXZ(positionAt(s - 1.5f), positionAt(s + 1.5f));
}

/* Unit normal to the LEFT of increasing s.  Facing (dx,dz) with +Y up, the
 * left side is (dz,-dx) - the same convention Scene uses for the verges. */
Vec3 RoadTrack::normalAt(float s) const
{
    const Vec3 a = positionAt(s - 1.5f);
    const Vec3 b = positionAt(s + 1.5f);

    const float dx  = b.x - a.x;
    const float dz  = b.z - a.z;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0e-4f) return Vec3(1.0f, 0.0f, 0.0f);

    return Vec3(dz / len, 0.0f, -dx / len);
}

float RoadTrack::halfWidthAt(float s) const
{
    if (mHalfW.empty()) return 4.0f;
    if (s <= 0.0f)      return mHalfW.front();
    if (s >= mLength)   return mHalfW.back();

    float walked = 0.0f;
    for (std::size_t i = 0; i < mSegLen.size(); ++i)
    {
        if (s <= walked + mSegLen[i])
        {
            const float t = (mSegLen[i] > 1.0e-5f)
                          ? (s - walked) / mSegLen[i] : 0.0f;
            const float a = mHalfW[i];
            const float b = mHalfW[(i + 1 < mHalfW.size()) ? i + 1 : i];
            return gh::lerp(a, b, t);
        }
        walked += mSegLen[i];
    }
    return mHalfW.back();
}

/* --------------------------------------------------------------------------
 *  How far each lane centre sits from the road centre line.
 *
 *  Two competing constraints, and the clamp honours both:
 *    - at least kLaneMin, or two carts meeting head on would overlap in the
 *      middle of the road;
 *    - at most halfWidth * 0.5, so a cart on a narrow stretch keeps its
 *      wheels on the dirt instead of riding the grass.
 *  The road narrows toward the far end, where those two pull in opposite
 *  directions.  kLaneMin wins: a cart with a wheel on the verge merely looks
 *  untidy, whereas two carts sharing one strip of road looks broken.
 * ------------------------------------------------------------------------ */
float RoadTrack::laneOffsetAt(float s) const
{
    /* Widest point of a cart is the wheel rim at 1.58 + half a rim voxel,
     * so 1.70.  Two lanes at +-2.15 leaves ~0.9 of daylight between passing
     * carts, which is enough that the pass reads as two vehicles sharing a
     * road rather than two vehicles scraping. */
    const float kLaneMin = 2.15f;
    const float half     = halfWidthAt(s);
    const float want     = half * 0.5f;
    return (want < kLaneMin) ? kLaneMin : want;
}

/* ==========================================================================
 *  Carriage
 * ==========================================================================*/
Carriage::Carriage()
    : mId(0)
    , mTrack(NULL)
    , mS(0.0f)
    , mSpeed(5.0f)
    , mLane(1)
    , mActive(false)
    , mSpeedNow(0.0f)
    , mHaltS(-1.0f)
    , mHaltTimer(0.0f)
    , mWheelSpin(0.0f)
    , mGaitPhase(0.0f)
    , mTailPhase(0.0f)
{
}

void Carriage::init(int id, const RoadTrack& track,
                    float speed, float startOffset, int lane)
{
    mId     = id;
    mTrack  = &track;
    mSpeed  = speed;
    mLane   = (lane >= 0) ? 1 : -1;
    mActive = true;

    /* startOffset is given as progress along this cart's OWN lane, so both
     * directions are seeded the same way; setProgress maps it to arc length. */
    setProgress(startOffset);

    mSpeedNow  = speed;
    mTailPhase = gh::randRange(0.0f, 6.28f);
    mGaitPhase = gh::randRange(0.0f, 6.28f);

    chooseNextHalt();
}

/* --------------------------------------------------------------------------
 *  Progress: how far this cart has travelled along its own lane, always
 *  increasing regardless of which way it drives.
 *
 *  Lane +1 runs with increasing arc length, lane -1 runs against it.  Working
 *  in progress rather than raw arc length means the whole following rule -
 *  the gap test, the ramp, the clamp, the park-at-the-end check - is written
 *  once and is direction agnostic.
 * ------------------------------------------------------------------------ */
float Carriage::progress() const
{
    if (mTrack == NULL) return mS;
    return (mLane > 0) ? mS : (mTrack->totalLength() - mS);
}

void Carriage::setProgress(float p)
{
    if (mTrack == NULL) { mS = p; return; }
    mS = (mLane > 0) ? p : (mTrack->totalLength() - p);
}

/* One stop per lap, somewhere along the market frontage. */
void Carriage::chooseNextHalt()
{
    if (mTrack == NULL) { mHaltS = -1.0f; return; }
    /* Expressed in progress, so both directions halt in the market rather
     * than one of them halting off camera at the far end. */
    mHaltS     = mTrack->totalLength() * gh::randRange(0.30f, 0.55f);
    mHaltTimer = 0.0f;
}

/* --------------------------------------------------------------------------
 *  Free road ahead of this cart, in world units.
 *
 *  Only carts in the SAME lane can be hit: opposing traffic is held apart by
 *  the lane offset itself, so a head-on pair is never even compared here.
 *  Within a lane nobody overtakes, so the only candidate is the nearest cart
 *  with greater progress.  The gap subtracts both vehicles' overhangs, so
 *  gap == 0 means bumper touching muzzle.  Returns a large number when the
 *  road ahead is empty.
 * ------------------------------------------------------------------------ */
float Carriage::gapToCarAhead(const std::vector<Carriage>& all) const
{
    const float mine = progress();
    float best = 1.0e9f;

    for (std::size_t i = 0; i < all.size(); ++i)
    {
        const Carriage& o = all[i];
        if (!o.mActive || o.mId == mId) continue;
        if (o.mLane != mLane) continue;            /* oncoming: different lane */

        const float theirs = o.progress();
        if (theirs <= mine) continue;              /* behind us */

        const float gap = (theirs - mine) - kCarLen;
        if (gap < best) best = gap;
    }
    return best;
}

void Carriage::update(float dt, const std::vector<Carriage>& all, float pedGap)
{
    if (!mActive || mTrack == NULL) return;

    /* The road ahead is blocked by whichever comes first: the cart in front,
     * or somebody on foot who has strayed onto the carriageway.  Taking the
     * smaller of the two means one ramp and one clamp cover both, so a driver
     * cannot brake for a cart and then drive through a person. */
    float gap = gapToCarAhead(all);
    if (pedGap < gap) gap = pedGap;

    /* ---- 1. what speed does the road allow? --------------------------- */
    float target = mSpeed;

    if (gap < kGapCruise)
    {
        /* Linear ramp from a dead stop at kGapStop to cruise at kGapCruise. */
        const float t = (gap - kGapStop) / (kGapCruise - kGapStop);
        target = mSpeed * std::max(0.0f, std::min(1.0f, t));
    }

    /* ---- 2. the market halt (EP3) ------------------------------------- */
    const float travelled = progress();

    if (mHaltTimer > 0.0f)
    {
        mHaltTimer -= dt;
        target = 0.0f;
    }
    else if (mHaltS >= 0.0f && travelled >= mHaltS)
    {
        mHaltTimer = gh::randRange(4.0f, 7.0f);   /* loading goods */
        mHaltS     = -1.0f;                       /* once per lap  */
        target     = 0.0f;
    }

    /* ---- 3. ease toward it, braking harder than accelerating ---------- */
    const float rate = (target < mSpeedNow) ? kBrake : kAccel;
    const float step = rate * dt;
    if (mSpeedNow < target) mSpeedNow = std::min(target, mSpeedNow + step);
    else                    mSpeedNow = std::max(target, mSpeedNow - step);
    if (mSpeedNow < 0.0f) mSpeedNow = 0.0f;

    /* ---- 4. move, but NEVER further than the gap that actually exists --
     * This clamp is what makes overlap impossible rather than unlikely: even
     * if the ramp above misjudged the braking distance, the cart physically
     * cannot advance past the vehicle in front of it. */
    float advance = mSpeedNow * dt;
    if (advance > gap)
    {
        advance   = std::max(0.0f, gap);
        mSpeedNow = 0.0f;                          /* hit the buffers */
    }
    setProgress(travelled + advance);

    /* ---- 5. animation, driven by distance covered so it never slides --- */
    if (kWheelR > 1.0e-4f)
        mWheelSpin += (advance / kWheelR) * 57.2958f;   /* rad -> deg */
    while (mWheelSpin > 360.0f) mWheelSpin -= 360.0f;

    mGaitPhase += advance * 0.62f;
    mTailPhase += dt * 1.4f;

    /* ---- 6. off the end of the road -> park until the entry is clear --- */
    if (progress() > mTrack->totalLength())
        mActive = false;
}

/* Scene calls this - and only when it has checked the road entry is clear. */
void Carriage::respawn()
{
    setProgress(0.0f);
    mSpeedNow  = mSpeed;
    mActive    = true;
    mHaltTimer = 0.0f;
    chooseNextHalt();
}

float Carriage::entryClearance() { return kEntryClear; }

float Carriage::noseAhead() { return kNoseAhead; }

/* --------------------------------------------------------------------------
 *  Where the body is, and which way it points.
 *
 *  Both mirror the transform in draw() exactly - lane offset included - so
 *  anything measured against these is measured against the cart that is on
 *  screen, not against an idealised point on the centre line.
 * ------------------------------------------------------------------------ */
Vec3 Carriage::worldPos() const
{
    if (mTrack == NULL) return Vec3();

    const Vec3  c   = mTrack->positionAt(mS);
    const Vec3  n   = mTrack->normalAt(mS);
    const float off = mTrack->laneOffsetAt(mS) * static_cast<float>(mLane);

    return Vec3(c.x + n.x * off, 0.0f, c.z + n.z * off);
}

Vec3 Carriage::forwardDir() const
{
    if (mTrack == NULL) return Vec3(0.0f, 0.0f, 1.0f);

    const float heading = mTrack->headingAt(mS) +
                          ((mLane > 0) ? 0.0f : 180.0f);
    const float a = heading * 0.01745329f;

    /* headingXZ() measures degrees about +Y with +Z at zero. */
    return Vec3(std::sin(a), 0.0f, std::cos(a));
}

/* ==========================================================================
 *  Draw
 * ==========================================================================*/
void Carriage::draw() const
{
    if (!mActive || mTrack == NULL) return;

    const Vec3  c = mTrack->positionAt(mS);
    const Vec3  n = mTrack->normalAt(mS);
    const float off = mTrack->laneOffsetAt(mS);

    /* Keep to your own left.  Lane +1 drives with increasing s, so its left is
     * the track's own left normal (+n).  Lane -1 drives the other way, so its
     * forward is reversed and its left becomes -n.  Both cases are therefore
     * off * mLane - which lands the two lanes on opposite sides of the centre
     * line, and that separation is what keeps oncoming carts apart. */
    const float lateral = off * static_cast<float>(mLane);

    /* Lane -1 faces backwards along the track. */
    const float heading = mTrack->headingAt(mS) +
                          ((mLane > 0) ? 0.0f : 180.0f);

    glPushMatrix();
        glTranslatef(c.x + n.x * lateral, 0.0f, c.z + n.z * lateral);
        glRotatef(heading, 0.0f, 1.0f, 0.0f);   /* authored facing +Z */

        drawCart();
        drawHorse();
    glPopMatrix();
}

/* --------------------------------------------------------------------------
 *  The cart: two midpoint-circle wheels, a plank bed, the driver's bench and
 *  the shafts that reach forward to the horse.
 * ------------------------------------------------------------------------ */
void Carriage::drawCart() const
{
    const int look = mId & 3;

    /* ---- 1. the two wheels (EP1 midpoint circle) --------------------- */
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
            glTranslatef(side * 1.58f, kWheelR, kAxleZ);

            /* The rim is drawn UNROTATED: rasterised voxels stepping round a
             * turning circle would shimmer.  Only the spokes turn, which is
             * what actually reads as rolling. */
            gh::drawCircleMidpoint3D(0.0f, 0.0f, 0.0f,
                                     static_cast<int>(kWheelVoxR), kWheelVox,
                                     gh::PLANE_YZ, kWheelRim);

            glRotatef(mWheelSpin, 1.0f, 0.0f, 0.0f);
            for (int k = 0; k < 2; ++k)
            {
                glPushMatrix();
                    glRotatef(k * 45.0f, 1.0f, 0.0f, 0.0f);
                    gh::drawBlock(0.16f, kWheelR * 1.78f, 0.16f, kSpokeCol);
                glPopMatrix();
            }
            gh::drawBlock(0.44f, 0.44f, 0.44f, kIron);   /* hub */
        glPopMatrix();
    }

    /* ---- 2. axle ------------------------------------------------------ */
    glPushMatrix();
        glTranslatef(0.0f, kWheelR, kAxleZ);
        gh::drawBlock(3.30f, 0.26f, 0.26f, kIron);
    glPopMatrix();

    /* ---- 3. bed, rails, boards --------------------------------------- */
    glPushMatrix();
        glTranslatef(0.0f, kBedY, -1.80f);
        gh::drawBlock(2.90f, 0.30f, 4.60f, kCartWood);
    glPopMatrix();

    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
            glTranslatef(side * 1.45f, kBedY + 0.60f, -1.80f);
            gh::drawBlock(0.20f, 0.90f, 4.60f, kCartRail);
        glPopMatrix();
    }
    glPushMatrix();                                   /* rear board */
        glTranslatef(0.0f, kBedY + 0.60f, -4.05f);
        gh::drawBlock(2.90f, 0.90f, 0.20f, kCartRail);
    glPopMatrix();
    glPushMatrix();                                   /* footboard  */
        glTranslatef(0.0f, kBedY + 0.55f, 0.42f);
        gh::drawBlock(2.90f, 0.80f, 0.22f, kCartRail);
    glPopMatrix();

    /* ---- 4. cargo: sacks and a couple of melons ---------------------- */
    for (int i = 0; i < 3; ++i)
    {
        glPushMatrix();
            glTranslatef(-0.80f + i * 0.80f, kBedY + 0.62f,
                         -3.30f + (i & 1) * 0.55f);
            gh::drawBlock(0.86f, 0.84f, 0.90f,
                          gh::shade(kSackCol, 0.92f + 0.08f * i));
        glPopMatrix();
    }
    for (int i = 0; i < 2; ++i)
    {
        gh::applyColor(Color(0.30f, 0.62f, 0.22f));
        glPushMatrix();
            glTranslatef(-0.45f + i * 0.95f, kBedY + 0.62f, -1.85f);
            glutSolidSphere(0.44, 10, 8);
        glPopMatrix();
    }

    /* ---- 5. driver's bench + the driver ------------------------------ */
    glPushMatrix();
        glTranslatef(0.0f, kBenchTop - 0.14f, -0.30f);
        gh::drawBlock(2.40f, 0.28f, 1.00f, kCartRail);
    glPopMatrix();

    {
        CustomerLook driver;
        driver.shirt    = kShirtCol[look];
        driver.trousers = Color(0.22f, 0.20f, 0.26f);
        driver.scale    = 0.94f;

        glPushMatrix();
            glTranslatef(0.0f, kBenchTop, -0.30f);
            glScalef(driver.scale, driver.scale, driver.scale);
            /* a slow rein-hand sway so the driver is not a statue */
            drawHumanSeated(driver, 5.0f * std::sin(mTailPhase * 0.8f));
        glPopMatrix();
    }

    /* ---- 6. shafts running forward to the horse's shoulders ---------- */
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
            glTranslatef(side * 0.86f, kBedY + 0.35f, 2.45f);
            glRotatef(-11.0f, 1.0f, 0.0f, 0.0f);
            gh::drawBlock(0.20f, 0.20f, 4.60f, kCartRail);
        glPopMatrix();
    }

    /* ---- 7. the tail lamp, lit after dusk ---------------------------- */
    const float night = gh::nightFactor();
    if (night > 0.02f)
    {
        glPushMatrix();
            glTranslatef(1.30f, kBedY + 1.35f, -4.00f);
            gh::drawBlock(0.16f, 0.50f, 0.16f, kIron);   /* bracket */

            gh::beginLampLight();
                glPushMatrix();
                    glTranslatef(0.0f, -0.34f, 0.0f);
                    gh::drawBlock(0.34f, 0.40f, 0.34f,
                                  gh::mixColor(Color(0.50f, 0.40f, 0.20f),
                                               Color(1.00f, 0.82f, 0.40f),
                                               night));
                glPopMatrix();
            gh::endLampLight();
        glPopMatrix();
    }
}

/* --------------------------------------------------------------------------
 *  The horse: barrel, neck, head and four legs, each rotating about its own
 *  hip in a diagonal trot (front-left swings with rear-right).
 * ------------------------------------------------------------------------ */
void Carriage::drawHorse() const
{
    const Color coat = kCoat[mId & 3];
    const Color mane = gh::shade(coat, 0.55f);

    const float swing = 26.0f * std::sin(mGaitPhase);

    glPushMatrix();
        glTranslatef(0.0f, 0.0f, kHorseZ);

        /* ---- barrel + chest ------------------------------------------ */
        glPushMatrix();
            glTranslatef(0.0f, kBarrelY, 0.0f);
            gh::drawBlock(1.45f, 1.75f, 3.50f, coat);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(0.0f, kBarrelY + 0.10f, 1.55f);
            gh::drawBlock(1.55f, 1.60f, 0.80f, gh::shade(coat, 1.05f));
        glPopMatrix();

        /* ---- harness strap ------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kBarrelY + 0.05f, 0.30f);
            gh::drawBlock(1.55f, 0.28f, 0.90f, Color(0.30f, 0.20f, 0.12f));
        glPopMatrix();

        /* ---- neck -> head (hierarchical: head hangs off the neck) -----
         * The neck leans well forward (kNeckLean) like a horse leaning into
         * a load, and the head frame then counter-rotates so the muzzle ends
         * up just below horizontal instead of pointing at the sky. */
        glPushMatrix();
            glTranslatef(0.0f, kBarrelY + 0.62f, 1.32f);
            glRotatef(kNeckLean, 1.0f, 0.0f, 0.0f);

            glPushMatrix();
                glTranslatef(0.0f, kNeckLen * 0.5f, 0.0f);
                gh::drawBlock(0.86f, kNeckLen, 1.05f, coat);
            glPopMatrix();

            /* mane down the back edge of the neck */
            glPushMatrix();
                glTranslatef(0.0f, kNeckLen * 0.52f, -0.52f);
                gh::drawBlock(0.44f, kNeckLen * 1.04f, 0.22f, mane);
            glPopMatrix();

            /* Head frame: total pitch = kNeckLean + this, so the muzzle sits
             * kMuzzlePitch degrees below level. */
            glPushMatrix();
                glTranslatef(0.0f, kNeckLen, 0.0f);
                glRotatef(kMuzzlePitch - kNeckLean, 1.0f, 0.0f, 0.0f);

                glPushMatrix();
                    glTranslatef(0.0f, 0.0f, 0.52f);
                    gh::drawBlock(0.78f, 0.82f, 1.60f, coat);
                glPopMatrix();
                glPushMatrix();                        /* muzzle */
                    glTranslatef(0.0f, -0.10f, 1.40f);
                    gh::drawBlock(0.62f, 0.55f, 0.42f, gh::shade(coat, 0.72f));
                glPopMatrix();
                for (int e = -1; e <= 1; e += 2)       /* ears */
                {
                    glPushMatrix();
                        glTranslatef(e * 0.26f, 0.55f, -0.10f);
                        gh::drawBlock(0.20f, 0.42f, 0.22f, mane);
                    glPopMatrix();
                }
            glPopMatrix();
        glPopMatrix();

        /* ---- tail ----------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kBarrelY + 0.55f, -1.72f);
            glRotatef(28.0f + 6.0f * std::sin(mTailPhase), 1.0f, 0.0f, 0.0f);
            glTranslatef(0.0f, -0.75f, 0.0f);
            gh::drawBlock(0.34f, 1.55f, 0.34f, mane);
        glPopMatrix();

        /* ---- four legs, diagonal trot -------------------------------- */
        for (int f = 0; f < 2; ++f)          /* 0 = rear, 1 = front */
        for (int side = -1; side <= 1; side += 2)
        {
            /* front-left moves with rear-right, so the pairs are opposed */
            const bool inPhase = ((f == 1) == (side < 0));
            const float a = inPhase ? swing : -swing;

            glPushMatrix();
                glTranslatef(side * 0.58f, kLegLen,
                             (f == 1) ? 1.25f : -1.25f);
                glRotatef(a, 1.0f, 0.0f, 0.0f);

                glPushMatrix();
                    glTranslatef(0.0f, -kLegLen * 0.5f, 0.0f);
                    gh::drawBlock(0.44f, kLegLen, 0.44f,
                                  gh::shade(coat, 0.94f));
                glPopMatrix();
                glPushMatrix();                        /* hoof */
                    glTranslatef(0.0f, -kLegLen + 0.14f, 0.02f);
                    gh::drawBlock(0.50f, 0.28f, 0.52f, kHoof);
                glPopMatrix();
            glPopMatrix();
        }

    glPopMatrix();
}
