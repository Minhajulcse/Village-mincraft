/* ==========================================================================
 *  Customer.cpp
 *  --------------------------------------------------------------------------
 *  EP3 : the 4-state path state machine.
 *  EP4 : hierarchical matrix stacks - the avatar is drawn as
 *
 *          body frame
 *            +-- head       (translate)
 *            +-- torso      (translate)
 *            +-- arm L / R  (translate to shoulder -> rotate -> drawBlock)
 *            +-- leg L / R  (translate to hip      -> rotate -> drawBlock)
 * ==========================================================================*/
#include "Customer.h"

#include <algorithm>
#include <cmath>

using gh::Vec3;
using gh::Color;

/* ==========================================================================
 *  Body proportions (world units, before mLook.scale)
 * ==========================================================================*/
namespace {

const float kLegH     = 1.70f;   /* hip pivot height                       */
const float kLegW     = 0.52f;
const float kLegD     = 0.52f;

const float kTorsoH   = 1.45f;
const float kTorsoW   = 1.15f;
const float kTorsoD   = 0.62f;

const float kHeadS    = 0.92f;   /* head is a cube                         */
const float kNeck     = 0.06f;

const float kArmH     = 1.35f;
const float kArmW     = 0.36f;
const float kArmD     = 0.40f;

const float kWalkSwingMax = 34.0f;   /* degrees */
const float kArriveEps    = 0.45f;   /* waypoint capture radius */

float wrapAngle(float deg)
{
    while (deg >  180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

} /* anonymous namespace */

/* ==========================================================================
 *  Construction / initialisation
 * ==========================================================================*/
Customer::Customer()
    : mId(0)
    , mTargetIdx(1)
    , mShopWaypoint(-1)
    , mHeading(0.0f)
    , mTargetHeading(0.0f)
    , mSpeed(3.0f)
    , mState(STATE_WALKING)
    , mStateTimer(0.0f)
    , mShopDuration(3.0f)
    , mSpawnDelay(0.0f)
    , mVisible(true)
    , mSwing(0.0f)
    , mSwingPhase(0.0f)
    , mBob(0.0f)
    , mBrowseLean(0.0f)
    , mFade(1.0f)
{
}

void Customer::init(int id,
                    const std::vector<Vec3>& path,
                    int shopWaypoint,
                    const CustomerLook& look,
                    float speed,
                    float spawnDelay)
{
    mId           = id;
    mPath         = path;
    mShopWaypoint = shopWaypoint;
    mLook         = look;
    mSpeed        = speed;
    mSpawnDelay   = spawnDelay;

    if (mPath.empty())
        mPath.push_back(Vec3(0.0f, 0.0f, 0.0f));

    resetToOrigin();

    /* Randomise the browse duration inside the required 2-4 second window. */
    mShopDuration = gh::randRange(2.0f, 4.0f);
    /* De-synchronise the limb swings between villagers. */
    mSwingPhase   = gh::randRange(0.0f, 6.28f);
}

void Customer::resetToOrigin()
{
    mPos       = mPath[0];
    mTargetIdx = (mPath.size() > 1) ? 1 : 0;
    mState     = STATE_WALKING;
    mStateTimer = 0.0f;
    mFade       = 1.0f;
    mVisible    = true;
    mBrowseLean = 0.0f;

    if (mPath.size() > 1)
        mHeading = gh::headingXZ(mPath[0], mPath[1]);
    mTargetHeading = mHeading;
}

/* ==========================================================================
 *  EP3 - state machine update
 * ==========================================================================*/
void Customer::update(float dt)
{
    /* Staggered spawn: hold at the path origin until the delay elapses. */
    if (mSpawnDelay > 0.0f)
    {
        mSpawnDelay -= dt;
        mVisible = false;
        return;
    }
    mVisible = (mState != STATE_DESPAWN);

    mStateTimer += dt;

    switch (mState)
    {
        /* ------------------------------------------------------------------
         *  STATE_WALKING - head for the next waypoint, swing the limbs.
         * ---------------------------------------------------------------- */
        case STATE_WALKING:
        {
            advanceAlongPath(dt);

            /* limb swing: sinusoidal, amplitude scales with walk speed */
            mSwingPhase += dt * mSpeed * 2.6f;
            mSwing = kWalkSwingMax * std::sin(mSwingPhase);
            mBob   = 0.055f * std::fabs(std::sin(mSwingPhase));

            /* ease the browse lean back to upright */
            mBrowseLean += (0.0f - mBrowseLean) * std::min(1.0f, dt * 6.0f);
            break;
        }

        /* ------------------------------------------------------------------
         *  STATE_SHOPPING - stand still 2-4 s, lean in and inspect goods.
         * ---------------------------------------------------------------- */
        case STATE_SHOPPING:
        {
            /* settle the limbs to rest */
            mSwing += (0.0f - mSwing) * std::min(1.0f, dt * 5.0f);
            mBob    = 0.0f;

            /* gentle browsing lean + a small look-around sway */
            const float lean = 9.0f + 3.0f * std::sin(mStateTimer * 2.2f);
            mBrowseLean += (lean - mBrowseLean) * std::min(1.0f, dt * 4.0f);
            mHeading    += std::sin(mStateTimer * 1.5f) * dt * 9.0f;

            if (mStateTimer >= mShopDuration)
            {
                /* Browsing finished -> walk the remaining waypoints out. */
                mState      = STATE_EXITING;
                mStateTimer = 0.0f;
                if (mTargetIdx < static_cast<int>(mPath.size()) - 1)
                    ++mTargetIdx;
            }
            break;
        }

        /* ------------------------------------------------------------------
         *  STATE_EXITING - identical locomotion, but no more shop stops.
         * ---------------------------------------------------------------- */
        case STATE_EXITING:
        {
            advanceAlongPath(dt);

            mSwingPhase += dt * mSpeed * 2.6f;
            mSwing = kWalkSwingMax * std::sin(mSwingPhase);
            mBob   = 0.055f * std::fabs(std::sin(mSwingPhase));

            mBrowseLean += (0.0f - mBrowseLean) * std::min(1.0f, dt * 6.0f);
            break;
        }

        /* ------------------------------------------------------------------
         *  STATE_DESPAWN - shrink away off-screen, then respawn at origin.
         * ---------------------------------------------------------------- */
        case STATE_DESPAWN:
        {
            mFade -= dt * 1.8f;
            if (mFade <= 0.0f)
            {
                resetToOrigin();
                /* fresh randomised browse time for the next lap */
                mShopDuration = gh::randRange(2.0f, 4.0f);
            }
            break;
        }
    }

    /* Smoothly turn toward the direction of travel. */
    const float delta = wrapAngle(mTargetHeading - mHeading);
    mHeading += delta * std::min(1.0f, dt * 5.0f);
}

/* --------------------------------------------------------------------------
 *  Shared locomotion for STATE_WALKING and STATE_EXITING.
 * ------------------------------------------------------------------------ */
void Customer::advanceAlongPath(float dt)
{
    if (mPath.empty()) return;

    const int last = static_cast<int>(mPath.size()) - 1;
    if (mTargetIdx > last)
    {
        mState      = STATE_DESPAWN;
        mStateTimer = 0.0f;
        return;
    }

    const Vec3& tgt = mPath[static_cast<size_t>(mTargetIdx)];

    const float dx   = tgt.x - mPos.x;
    const float dz   = tgt.z - mPos.z;
    const float dist = std::sqrt(dx * dx + dz * dz);

    if (dist <= kArriveEps)
    {
        mPos.x = tgt.x;
        mPos.z = tgt.z;
        arriveAtWaypoint();
        return;
    }

    const float step = mSpeed * dt;
    mPos.x += (dx / dist) * step;
    mPos.z += (dz / dist) * step;
    mPos.y  = tgt.y;                        /* follow waypoint elevation */

    mTargetHeading = gh::headingXZ(Vec3(mPos.x, 0.0f, mPos.z),
                                   Vec3(tgt.x, 0.0f, tgt.z));
}

void Customer::arriveAtWaypoint()
{
    const int last = static_cast<int>(mPath.size()) - 1;

    /* Reached the stall-front waypoint while still on the inbound leg? */
    if (mState == STATE_WALKING && mTargetIdx == mShopWaypoint)
    {
        mState      = STATE_SHOPPING;
        mStateTimer = 0.0f;
        /* Face the stall: the waypoint after the stall-front is the way
         * back to the path, so look opposite to it (i.e. at the counter). */
        if (mTargetIdx < last)
        {
            const float away = gh::headingXZ(mPath[static_cast<size_t>(mTargetIdx)],
                                             mPath[static_cast<size_t>(mTargetIdx + 1)]);
            mTargetHeading = away + 180.0f;
        }
        return;
    }

    if (mTargetIdx >= last)
    {
        /* End of the path -> vanish and queue a respawn. */
        mState      = STATE_DESPAWN;
        mStateTimer = 0.0f;
        return;
    }

    ++mTargetIdx;
    if (mState == STATE_WALKING && mTargetIdx > mShopWaypoint && mShopWaypoint >= 0)
        mState = STATE_EXITING;   /* stall already passed */
}

/* ==========================================================================
 *  EP4 - the shared six-cuboid human
 *
 *      hip frame (lean)
 *        +-- torso      (translate      -> drawBlock)
 *        +-- head       (translate      -> drawBlock)
 *        +-- arm L / R  (shoulder pivot -> rotate -> drawBlock)
 *        +-- leg L / R  (hip pivot      -> rotate -> drawBlock)
 * ==========================================================================*/
void drawHuman(const CustomerLook& look,
               float armSwing,
               float legSwing,
               float lean)
{
    glPushMatrix();

        /* the whole body leans forward about the hips while browsing */
        glTranslatef(0.0f, kLegH, 0.0f);
        glRotatef(lean, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -kLegH, 0.0f);

        /* ---- 1. torso ------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kLegH + kTorsoH * 0.5f, 0.0f);
            gh::drawBlock(kTorsoW, kTorsoH, kTorsoD, look.shirt);
        glPopMatrix();

        /* ---- 2. head -------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kLegH + kTorsoH + kNeck + kHeadS * 0.5f, 0.0f);
            gh::drawBlock(kHeadS, kHeadS, kHeadS, look.skin);
        glPopMatrix();

        /* ---- 3, 4. arms : swing opposite to the leg on the same side --- */
        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
                glTranslatef(side * (kTorsoW * 0.5f + kArmW * 0.5f),
                             kLegH + kTorsoH - 0.10f, 0.0f);
                glRotatef(-side * armSwing, 1.0f, 0.0f, 0.0f);
                glTranslatef(0.0f, -kArmH * 0.5f, 0.0f);
                gh::drawBlock(kArmW, kArmH, kArmD, look.shirt);
            glPopMatrix();
        }

        /* ---- 5, 6. legs ----------------------------------------------- */
        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
                glTranslatef(side * (kLegW * 0.52f), kLegH, 0.0f);
                glRotatef(side * legSwing, 1.0f, 0.0f, 0.0f);
                glTranslatef(0.0f, -kLegH * 0.5f, 0.0f);
                gh::drawBlock(kLegW, kLegH, kLegD, look.trousers);
            glPopMatrix();
        }

    glPopMatrix();
}

/* ==========================================================================
 *  EP4 - the seated variant, for the carriage drivers
 *
 *      hip frame (the caller puts this ON the bench, facing +Z)
 *        +-- torso        (translate -> drawBlock)
 *        +-- head         (translate -> drawBlock)
 *        +-- arm L / R    (shoulder pivot -> rotate forward -> drawBlock)
 *        +-- thigh L / R  (hip pivot -> rotate to horizontal -> drawBlock)
 *              +-- shin   (knee pivot -> rotate back to vertical -> drawBlock)
 *
 *  Same proportions table as drawHuman(): only the joint angles differ, and
 *  the single leg block becomes a thigh + shin pair so the knee can bend.
 *  Both arms swing the SAME way here (a driver holds the reins with both
 *  hands) rather than the mirrored gait drawHuman() uses.
 * ==========================================================================*/
void drawHumanSeated(const CustomerLook& look, float armSwing)
{
    const float thighL = kLegH * 0.53f;
    const float shinL  = kLegH * 0.47f;

    glPushMatrix();

        /* ---- 1. torso ------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kTorsoH * 0.5f, 0.0f);
            gh::drawBlock(kTorsoW, kTorsoH, kTorsoD, look.shirt);
        glPopMatrix();

        /* ---- 2. head -------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kTorsoH + kNeck + kHeadS * 0.5f, 0.0f);
            gh::drawBlock(kHeadS, kHeadS, kHeadS, look.skin);
        glPopMatrix();

        /* ---- 3, 4. arms : both reaching forward for the reins --------- */
        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
                glTranslatef(side * (kTorsoW * 0.5f + kArmW * 0.5f),
                             kTorsoH - 0.10f, 0.0f);
                glRotatef(-(58.0f + armSwing), 1.0f, 0.0f, 0.0f);
                glTranslatef(0.0f, -kArmH * 0.5f, 0.0f);
                gh::drawBlock(kArmW, kArmH, kArmD, look.shirt);
            glPopMatrix();
        }

        /* ---- 5, 6. legs : thigh forward, shin down -------------------- *
         * A -90 deg turn about +X swings the limb's own -Y axis round to
         * +Z, i.e. straight out in front of the sitter; the knee then puts
         * the shin back to vertical. */
        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
                glTranslatef(side * (kLegW * 0.52f), 0.0f, 0.0f);
                glRotatef(-86.0f, 1.0f, 0.0f, 0.0f);

                glPushMatrix();                       /* thigh */
                    glTranslatef(0.0f, -thighL * 0.5f, 0.0f);
                    gh::drawBlock(kLegW, thighL, kLegD, look.trousers);
                glPopMatrix();

                glTranslatef(0.0f, -thighL, 0.0f);    /* knee  */
                glRotatef(86.0f, 1.0f, 0.0f, 0.0f);
                glTranslatef(0.0f, -shinL * 0.5f, 0.0f);
                gh::drawBlock(kLegW, shinL, kLegD,
                              gh::shade(look.trousers, 0.92f));
            glPopMatrix();
        }

    glPopMatrix();
}

/* ==========================================================================
 *  Customer draw - world transform, then the shared model
 * ==========================================================================*/
void Customer::draw() const
{
    if (!mVisible && mState != STATE_DESPAWN) return;
    if (mState == STATE_DESPAWN && mFade <= 0.0f) return;

    const float s = mLook.scale * ((mState == STATE_DESPAWN)
                                       ? std::max(0.0f, mFade)
                                       : 1.0f);
    if (s <= 0.001f) return;

    glPushMatrix();
        glTranslatef(mPos.x, mPos.y + mBob, mPos.z);
        glRotatef(mHeading, 0.0f, 1.0f, 0.0f);
        glScalef(s, s, s);
        drawHuman(mLook, mSwing, mSwing, mBrowseLean);
    glPopMatrix();
}
