/* ==========================================================================
 *  Stall.cpp
 *  --------------------------------------------------------------------------
 *  ONE parameterised renderer, drawGenericStall(), draws every stall in the
 *  bazar.  It replaces the former per-trade modelling code, which gave
 *  carrots, radishes, eggplants, melons, pumpkins, apples, teapots and cups
 *  a hand-written function each.
 *
 *  The vendor is the very same six-cuboid drawHuman() the walking customers
 *  use, driven with legSwing = 0 so shopkeepers stand still behind their
 *  counters instead of walking on the spot.
 * ==========================================================================*/
#include "Stall.h"
#include "Customer.h"                  /* drawHuman() + CustomerLook */

#include <algorithm>
#include <cmath>

using gh::Color;
using gh::Vec3;

/* ==========================================================================
 *  Shared palette + one footprint for every stall
 * ==========================================================================*/
namespace {

const Color kWoodDark  (0.42f, 0.26f, 0.13f);   /* posts / frame        */
const Color kWoodMid   (0.55f, 0.35f, 0.17f);   /* counter apron        */
const Color kWoodLight (0.72f, 0.51f, 0.26f);   /* counter top plank    */
const Color kSmokeCol  (0.88f, 0.88f, 0.90f);

const float kW        = 9.0f;    /* stall width          */
const float kD        = 4.4f;    /* stall depth          */
const float kCounterH = 2.50f;
const float kPostH    = 6.60f;

/* A four-sided pyramid standing on y = 0 - the "carrot" produce shape. */
void drawPyramid(float base, float height, const Color& c)
{
    const float b = base * 0.5f;
    const float vx[4] = { -b,  b,  b, -b };
    const float vz[4] = { -b, -b,  b,  b };
    const float face[4] = { 1.00f, 0.85f, 0.70f, 0.60f };

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 4; ++i)
    {
        const int j = (i + 1) & 3;
        gh::applyColor(gh::shade(c, face[i]));
        glVertex3f(vx[i], 0.0f,   vz[i]);
        glVertex3f(vx[j], 0.0f,   vz[j]);
        glVertex3f(0.0f,  height, 0.0f);
    }
    glEnd();
}

/* The goods: a 4 x 2 grid on the counter top, shape chosen by the caller. */
void drawProduceGrid(int shape)
{
    const float y  = kCounterH + 0.25f;
    /* Both rows must land on the top plank (which spans kD*0.5-1.95 ..
     * kD*0.5+0.05); the back row used to sit 0.6 behind it and hung in mid
     * air over the empty space between the counter and the vendor. */
    const float z0 = kD * 0.5f - 0.40f;

    for (int r = 0; r < 2; ++r)
    for (int c = 0; c < 4; ++c)
    {
        glPushMatrix();
            glTranslatef(-kW * 0.5f + 1.5f + c * 2.0f, y, z0 - r * 1.10f);

            if (shape == PRODUCE_SPHERES)
            {
                /* alternating yellow and red fruit */
                const Color f = ((r + c) & 1) ? Color(0.94f, 0.79f, 0.13f)
                                              : Color(0.86f, 0.16f, 0.13f);
                gh::applyColor(f);
                glTranslatef(0.0f, 0.42f, 0.0f);
                glutSolidSphere(0.42, 10, 8);
            }
            else if (shape == PRODUCE_CUBES)
            {
                glTranslatef(0.0f, 0.38f, 0.0f);
                gh::drawBlock(0.74f, 0.74f, 0.74f, Color(0.55f, 0.28f, 0.72f));
            }
            else
            {
                drawPyramid(0.78f, 1.05f, Color(0.95f, 0.52f, 0.10f));
            }
        glPopMatrix();
    }
}

} /* anonymous namespace */

/* ==========================================================================
 *  The single stall renderer.  Everything is authored around a local origin
 *  so it can be called standalone for scenery or from MarketStall::draw()
 *  inside an already-rotated frame.
 * ==========================================================================*/
void drawGenericStall(float x, float z,
                      float canopyR, float canopyG, float canopyB,
                      int produceShapeType)
{
    const Color canopy(canopyR, canopyG, canopyB);

    glPushMatrix();
    glTranslatef(x, 0.0f, z);

        /* ---- four corner posts ---------------------------------------- */
        for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
        {
            glPushMatrix();
                glTranslatef(sx * (kW * 0.5f - 0.30f), kPostH * 0.5f,
                             sz * (kD * 0.5f - 0.25f));
                gh::drawBlock(0.42f, kPostH, 0.42f, kWoodDark);
            glPopMatrix();
        }

        /* ---- counter: apron + top plank ------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, kCounterH * 0.5f, kD * 0.5f - 0.95f);
            gh::drawBlock(kW - 0.5f, kCounterH, 1.70f, kWoodMid);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(0.0f, kCounterH + 0.12f, kD * 0.5f - 0.95f);
            gh::drawBlock(kW, 0.24f, 2.00f, kWoodLight);
        glPopMatrix();

        if (produceShapeType == PRODUCE_TEA)
        {
            /* ---- tea variant: close three sides, pitch a gable roof ---- */
            const Color wall(0.46f, 0.33f, 0.19f);

            glPushMatrix();
                glTranslatef(0.0f, kPostH * 0.5f, -kD * 0.5f + 0.20f);
                gh::drawBlock(kW, kPostH, 0.40f, gh::shade(wall, 0.80f));
            glPopMatrix();
            for (int s = -1; s <= 1; s += 2)
            {
                glPushMatrix();
                    glTranslatef(s * (kW * 0.5f - 0.20f), kPostH * 0.5f, 0.0f);
                    gh::drawBlock(0.40f, kPostH, kD, wall);
                glPopMatrix();
            }

            glPushMatrix();
                glTranslatef(0.0f, kPostH, 0.0f);
                glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
                gh::drawRoofPrism(0.0f, 0.0f, 0.0f,
                                  kD + 1.8f, 2.40f, kW + 1.6f,
                                  canopy, gh::shade(canopy, 0.85f));
            glPopMatrix();

            /* kettle + a row of cups, all plain cuboids */
            glPushMatrix();
                glTranslatef(-kW * 0.5f + 1.7f, kCounterH + 0.72f,
                             kD * 0.5f - 1.10f);
                gh::drawBlock(1.10f, 0.95f, 1.10f, Color(0.30f, 0.30f, 0.33f));
                glPushMatrix();
                    glTranslatef(0.0f, 0.58f, 0.0f);
                    gh::drawBlock(0.42f, 0.24f, 0.42f, Color(0.18f, 0.18f, 0.20f));
                glPopMatrix();
            glPopMatrix();
            for (int i = 0; i < 4; ++i)
            {
                glPushMatrix();
                    glTranslatef(-0.4f + i * 0.90f, kCounterH + 0.42f,
                                 kD * 0.5f - 1.10f);
                    gh::drawBlock(0.40f, 0.34f, 0.40f, Color(0.93f, 0.92f, 0.88f));
                glPopMatrix();
            }

            /* Two benches out front, laid out as in the reference: a long
             * taller one across the stall front, with a shorter lower one
             * parked ahead of it and offset to the left.  Both are the same
             * plain timber - a flat top plank on two square legs. */
            const float benchLen [2] = { 7.20f, 4.30f };
            const float benchTop [2] = { 1.55f, 1.05f };   /* plank top    */
            const float benchDeep[2] = { 1.90f, 1.30f };   /* Z span       */
            const float benchX   [2] = { 1.10f, -2.40f };
            const float benchZ   [2] = { kD * 0.5f + 2.6f, kD * 0.5f + 5.1f };

            for (int b = 0; b < 2; ++b)
            {
                const float plank = 0.30f;
                const float legH  = benchTop[b] - plank;

                glPushMatrix();
                    glTranslatef(benchX[b], 0.0f, benchZ[b]);

                    /* top plank */
                    glPushMatrix();
                        glTranslatef(0.0f, benchTop[b] - plank * 0.5f, 0.0f);
                        gh::drawBlock(benchLen[b], plank, benchDeep[b],
                                      kWoodLight);
                    glPopMatrix();

                    /* one square leg slab under each end */
                    for (int l = -1; l <= 1; l += 2)
                    {
                        glPushMatrix();
                            glTranslatef(l * (benchLen[b] * 0.5f - 0.55f),
                                         legH * 0.5f, 0.0f);
                            gh::drawBlock(0.42f, legH, benchDeep[b] - 0.40f,
                                          gh::shade(kWoodMid, 0.92f));
                        glPopMatrix();
                    }
                glPopMatrix();
            }
        }
        else
        {
            /* ---- flat canopy + hanging valance ------------------------- */
            glPushMatrix();
                glTranslatef(0.0f, kPostH + 0.20f, 0.0f);
                gh::drawBlock(kW + 1.4f, 0.36f, kD + 1.6f, canopy);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(0.0f, kPostH - 0.18f, (kD + 1.6f) * 0.5f);
                gh::drawBlock(kW + 1.4f, 0.60f, 0.18f, gh::shade(canopy, 0.80f));
            glPopMatrix();

            drawProduceGrid(produceShapeType);
        }

    glPopMatrix();
}

/* ==========================================================================
 *  MarketStall
 * ==========================================================================*/
MarketStall::MarketStall()
    : mType(STALL_VEGETABLE)
    , mProduce(PRODUCE_PYRAMIDS)
    , mX(0.0f), mZ(0.0f), mRotY(0.0f)
    , mCanopy(0.80f, 0.20f, 0.16f)
    , mOwnerShirt(0.95f, 0.55f, 0.12f)
    , mDepth(kD)
    , mOwnerPhase(0.0f)
    , mOwnerArm(0.0f)
    , mSpawnTimer(0.0f)
{
}

void MarketStall::init(StallType type, float x, float z, float rotY,
                       const Color& canopy, const Color& ownerShirt)
{
    mType       = type;
    mX          = x;
    mZ          = z;
    mRotY       = rotY;
    mCanopy     = canopy;
    mOwnerShirt = ownerShirt;
    mDepth      = kD;

    switch (type)
    {
        case STALL_MELON:
        case STALL_FRUIT: mProduce = PRODUCE_SPHERES;  break;
        case STALL_CRATE: mProduce = PRODUCE_CUBES;    break;
        case STALL_TEA:   mProduce = PRODUCE_TEA;      break;
        default:          mProduce = PRODUCE_PYRAMIDS; break;
    }

    /* de-synchronise the idle sway between vendors */
    mOwnerPhase = gh::randRange(0.0f, 6.28f);
}

void MarketStall::update(float dt)
{
    /* Idle arm sway only - deliberately NOT a walk cycle. */
    mOwnerPhase += dt * 1.6f;
    mOwnerArm    = 11.0f * std::sin(mOwnerPhase);

    if (mType != STALL_TEA) return;

    /* ---- chai steam: throttled spawn, compacted retire, ~8 puffs max --- */
    mSpawnTimer -= dt;
    if (mSpawnTimer <= 0.0f)
    {
        mSpawnTimer = gh::randRange(0.22f, 0.38f);

        SmokePuff p;
        p.y     = 0.0f;
        p.life  = 1.0f;
        p.scale = gh::randRange(0.45f, 0.70f);
        p.drift = gh::randRange(-0.35f, 0.35f);
        p.spin  = gh::randRange(0.0f, 360.0f);
        mSmoke.push_back(p);
    }

    std::size_t w = 0;
    for (std::size_t i = 0; i < mSmoke.size(); ++i)
    {
        SmokePuff& p = mSmoke[i];
        p.y     += dt * 1.45f;
        p.scale += dt * 0.45f;
        p.drift += dt * 0.30f;
        p.spin  += dt * 24.0f;
        p.life  -= dt * 0.42f;
        if (p.life > 0.0f) mSmoke[w++] = p;
    }
    mSmoke.resize(w);
}

void MarketStall::draw() const
{
    glPushMatrix();                       /* ---- stall local frame ------- */
        glTranslatef(mX, 0.0f, mZ);
        glRotatef(mRotY, 0.0f, 1.0f, 0.0f);

        drawGenericStall(0.0f, 0.0f,
                         mCanopy.r, mCanopy.g, mCanopy.b, mProduce);

        /* ---- the vendor: the customers' own six cuboids --------------- */
        CustomerLook look;
        look.shirt    = mOwnerShirt;
        look.trousers = Color(0.20f, 0.19f, 0.26f);

        glPushMatrix();
            /* Behind the counter, authored facing +Z - the side the
             * customers browse from (see customerSpot()). */
            glTranslatef(-kW * 0.14f, 0.0f,
                         (mType == STALL_TEA) ? -0.55f : -(kD * 0.5f + 0.85f));
            /* legSwing = 0: shopkeepers never walk on the spot. */
            drawHuman(look, mOwnerArm, 0.0f, 0.0f);
        glPopMatrix();

        if (mType == STALL_TEA) drawSmoke();

        /* ---- the hurricane lamp, lit only after dusk ------------------- */
        drawLamp();
    glPopMatrix();                        /* ------------------------------ */
}

/* --------------------------------------------------------------------------
 *  A paraffin lamp hung from the canopy frame.  The bracket and the glass are
 *  ordinary tinted voxels until dusk; from then on the flame is drawn inside
 *  a lamp-light bracket so it keeps its own brightness while the rest of the
 *  market darkens around it.
 * ------------------------------------------------------------------------ */
void MarketStall::drawLamp() const
{
    const float night = gh::nightFactor();
    if (night <= 0.02f) return;           /* daylight: nothing to see */

    /* Tea stalls are closed on three sides, so their lamp hangs at the front
     * edge of the roof rather than under an open canopy. */
    const float lampY = kPostH - 0.75f;
    const float lampZ = (mType == STALL_TEA) ? (kD * 0.5f + 0.55f)
                                             : (kD * 0.5f - 0.30f);
    const float lampX = kW * 0.5f - 1.10f;

    glPushMatrix();
        glTranslatef(lampX, lampY, lampZ);

        /* hanging hook */
        glPushMatrix();
            glTranslatef(0.0f, 0.62f, 0.0f);
            gh::drawBlock(0.10f, 0.70f, 0.10f, Color(0.24f, 0.22f, 0.20f));
        glPopMatrix();

        /* brass cap + base */
        gh::drawBlock(0.46f, 0.14f, 0.46f, Color(0.42f, 0.34f, 0.18f));
        glPushMatrix();
            glTranslatef(0.0f, -0.70f, 0.0f);
            gh::drawBlock(0.46f, 0.14f, 0.46f, Color(0.42f, 0.34f, 0.18f));
        glPopMatrix();

        /* ---- the flame itself: exempt from the sunlight tint ----------- */
        gh::beginLampLight();
            const Color glow = gh::mixColor(Color(0.55f, 0.45f, 0.22f),
                                            Color(1.00f, 0.86f, 0.46f), night);
            glPushMatrix();
                glTranslatef(0.0f, -0.35f, 0.0f);
                gh::drawBlock(0.38f, 0.56f, 0.38f, glow);
            glPopMatrix();
        gh::endLampLight();
    glPopMatrix();
}

/* ---- rising steam above the kettle -------------------------------------- */
void MarketStall::drawSmoke() const
{
    if (mSmoke.empty()) return;

    const bool outline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const float baseX = -kW * 0.5f + 1.7f;
    const float baseY = kCounterH + 1.55f;
    const float baseZ = kD * 0.5f - 1.10f;

    for (std::size_t i = 0; i < mSmoke.size(); ++i)
    {
        const SmokePuff& p = mSmoke[i];
        /* Blended, so the tint has to be folded into the RGB by hand - the
         * alpha stays the puff's own fade. */
        const Color c = gh::tinted(gh::mixColor(kSmokeCol,
                                                Color(0.68f, 0.82f, 0.95f),
                                                1.0f - p.life));
        glPushMatrix();
            glTranslatef(baseX + p.drift * 0.55f, baseY + p.y,
                         baseZ + p.drift * 0.22f);
            glRotatef(p.spin, 0.0f, 1.0f, 0.0f);
            glColor4f(c.r, c.g, c.b, std::max(0.0f, p.life * 0.55f));
            glutSolidCube(p.scale);
        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    gh::setOutlineEnabled(outline);
}

/* Customers browse from the +Z (camera) side of the counter. */
Vec3 MarketStall::customerSpot() const
{
    const float a  = mRotY * static_cast<float>(M_PI) / 180.0f;
    const float lz = mDepth * 0.5f + 2.6f;
    return Vec3(mX + std::sin(a) * lz, 0.0f, mZ + std::cos(a) * lz);
}
