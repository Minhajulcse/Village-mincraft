/* ==========================================================================
 *  Stall.h
 *  --------------------------------------------------------------------------
 *  One parameterised market stall.  Every stall in the bazar - vegetable,
 *  melon, fruit, crate and tea - is drawn by the single function
 *  drawGenericStall(); only the canopy colour and the produce shape differ.
 * ==========================================================================*/
#ifndef STALL_H
#define STALL_H

#include "GraphicsHelpers.h"

#include <vector>

/* Which trade the stall plies (selects canopy colour + produce shape). */
enum StallType
{
    STALL_VEGETABLE = 0,
    STALL_MELON,
    STALL_FRUIT,
    STALL_CRATE,
    STALL_TEA
};

/* The produce laid out on the counter. */
enum ProduceShape
{
    PRODUCE_SPHERES  = 0,   /* yellow / red spheres - apples and melons  */
    PRODUCE_CUBES    = 1,   /* purple cubes - boxes and crates           */
    PRODUCE_PYRAMIDS = 2,   /* orange pyramids - carrots                 */
    PRODUCE_TEA      = 3    /* tea variant: hut walls + kettle, no grid   */
};

/* One rising puff above the chai fire. */
struct SmokePuff
{
    float y, life, scale, drift, spin;
    SmokePuff() : y(0.0f), life(0.0f), scale(1.0f), drift(0.0f), spin(0.0f) {}
};

/* --------------------------------------------------------------------------
 *  The one and only stall renderer.  Draws a complete stall centred on
 *  (x, z): four posts, a counter, a canopy tinted (canopyR, canopyG, canopyB)
 *  and a grid of produce chosen by produceShapeType (see ProduceShape).
 * ------------------------------------------------------------------------ */
void drawGenericStall(float x, float z,
                      float canopyR, float canopyG, float canopyB,
                      int produceShapeType);

class MarketStall
{
public:
    MarketStall();

    void init(StallType type, float x, float z, float rotY,
              const gh::Color& canopy, const gh::Color& ownerShirt);

    void update(float dt);
    void draw() const;

    /* Where a browsing customer should stand: in front of the counter. */
    gh::Vec3 customerSpot() const;

    StallType type() const { return mType; }

private:
    void drawSmoke() const;

    /* Hurricane lamp under the canopy - drawn only once night falls. */
    void drawLamp()  const;

    StallType  mType;
    int        mProduce;          /* ProduceShape derived from mType */
    float      mX, mZ, mRotY;
    gh::Color  mCanopy;
    gh::Color  mOwnerShirt;
    float      mDepth;

    float      mOwnerPhase;       /* idle arm sway - never a walk cycle */
    float      mOwnerArm;

    std::vector<SmokePuff> mSmoke;
    float                  mSpawnTimer;
};

#endif /* STALL_H */
