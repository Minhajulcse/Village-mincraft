// // /* ==========================================================================
// //  *  Scene.cpp
// //  *  --------------------------------------------------------------------------
// //  *  The whole rural market environment, assembled from the voxel primitives in
// //  *  GraphicsHelpers, plus the master updateScene() / drawScene() orchestration.
// //  * ==========================================================================*/
// // #include "Scene.h"

// // #include <algorithm>
// // #include <cmath>
// // #include <cstdio>
// // #include <cstdlib>
// // #include <fstream>
// // #include <string>

// // /* --------------------------------------------------------------------------
// //  *  Platform audio headers
// //  * ------------------------------------------------------------------------ */
// // #if defined(_WIN32)
// // #  include <windows.h>
// // #  include <mmsystem.h>
// // #  ifdef _MSC_VER
// // #    pragma comment(lib, "winmm.lib")
// // #  endif
// // #else
// // #  include <signal.h>
// // #  include <sys/types.h>
// // #  include <sys/wait.h>
// // #  include <unistd.h>
// // #endif

// // using gh::Color;
// // using gh::Vec3;

// // /* ==========================================================================
// //  *  Scene palette - taken straight from the reference artwork
// //  * ==========================================================================*/
// // namespace {

// // const Color kGrass      (0.45f, 0.75f, 0.15f);   /* spec: Minecraft grass  */
// // const Color kGrassDeep  (0.34f, 0.60f, 0.12f);
// // const Color kDirtPath   (0.85f, 0.70f, 0.35f);   /* spec: tan dirt path    */
// // const Color kDirtEdge   (0.70f, 0.55f, 0.26f);
// // const Color kStone      (0.58f, 0.58f, 0.57f);   /* pebbles                */
// // const Color kFenceWood  (0.48f, 0.31f, 0.16f);
// // const Color kTrunk      (0.42f, 0.28f, 0.15f);
// // const Color kLeafMid    (0.24f, 0.60f, 0.16f);
// // const Color kLeafDark   (0.16f, 0.44f, 0.12f);
// // const Color kHedge      (0.19f, 0.47f, 0.13f);

// // /* Village houses - cream rendered walls, thatch roof, stone plinth. */
// // const Color kHouseWall  (0.91f, 0.88f, 0.78f);   /* cream render          */
// // const Color kHouseTrim  (0.55f, 0.52f, 0.46f);   /* gable-end grey band   */
// // const Color kThatch     (0.87f, 0.72f, 0.28f);   /* straw roof            */
// // const Color kHouseDoor  (0.47f, 0.29f, 0.14f);   /* stained timber door   */
// // const Color kWindowDark (0.24f, 0.22f, 0.26f);   /* glazing behind bars   */
// // const Color kPlinth     (0.60f, 0.60f, 0.59f);   /* stone base + step     */

// // const Color kSunYellow  (1.00f, 0.86f, 0.12f);
// // const Color kSunRay     (1.00f, 0.93f, 0.42f);
// // const Color kCloudWhite (0.99f, 0.99f, 1.00f);
// // const Color kSkyTop     (0.16f, 0.55f, 0.95f);
// // const Color kSkyLow     (0.62f, 0.85f, 0.99f);

// // /* Terrain extents (world units). */
// // const float kGroundMinX = -90.0f;
// // const float kGroundMaxX =  90.0f;
// // const float kGroundMinZ = -56.0f;
// // const float kGroundMaxZ =  70.0f;

// // /* --------------------------------------------------------------------------
// //  *  Deterministic hash noise - lets the scattered props be regenerated
// //  *  identically every frame without storing them (and keeps display lists
// //  *  stable).
// //  * ------------------------------------------------------------------------ */
// // float hashNoise(int a, int b)
// // {
// //     unsigned int h = static_cast<unsigned int>(a) * 374761393u +
// //                      static_cast<unsigned int>(b) * 668265263u;
// //     h = (h ^ (h >> 13)) * 1274126177u;
// //     h ^= (h >> 16);
// //     return static_cast<float>(h % 10000u) / 10000.0f;   /* [0,1) */
// // }

// // /* Catmull-Rom spline sample - used to build the S-curved dirt path. */
// // Vec3 catmullRom(const Vec3& p0, const Vec3& p1,
// //                 const Vec3& p2, const Vec3& p3, float t)
// // {
// //     const float t2 = t * t;
// //     const float t3 = t2 * t;

// //     Vec3 r;
// //     r.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
// //                   (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
// //                   (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
// //     r.y = 0.0f;
// //     r.z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
// //                   (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
// //                   (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);
// //     return r;
// // }

// // float lerpF(float a, float b, float t) { return a + (b - a) * t; }

// // /* ==========================================================================
// //  *  Audio backend
// //  *  --------------------------------------------------------------------------
// //  *  One continuous ambient bed (breeze + birdsong) looping forever.  There are
// //  *  no farmyard one-shots: the scene has no poultry in it, so a rooster or a
// //  *  clucking hen only ever sounded like it came from somewhere else.
// //  *
// //  *  The WAV lives in assets/ next to the sources, but CMake copies it beside
// //  *  the executable too, so both `./rural_market` (repo root) and
// //  *  `./build/rural_market` find it - hence the two-place lookup below.
// //  * ==========================================================================*/
// // const char* kAmbientName = "village_ambient.wav";

// // #if !defined(_WIN32)
// // pid_t gAudioPid = -1;                /* the looping ambient bed */
// // #endif

// // bool fileExists(const std::string& path)
// // {
// //     std::ifstream f(path.c_str(), std::ios::binary);
// //     return f.good();
// // }

// // /* Return "assets/<name>" or "<name>", whichever exists; empty if neither. */
// // std::string findAsset(const char* name)
// // {
// //     std::string candidate = std::string("assets/") + name;
// //     if (fileExists(candidate)) return candidate;
// //     candidate = name;
// //     if (fileExists(candidate)) return candidate;
// //     return std::string();
// // }

// // #if !defined(_WIN32)
// // /* --------------------------------------------------------------------------
// //  *  Reap finished one-shot players.  Without this every clip would leave a
// //  *  zombie behind and a long session would slowly fill the process table -
// //  *  the same class of slow leak the HUD string builder used to be.
// //  *
// //  *  A *handler* is used rather than SIGCHLD = SIG_IGN on purpose: an ignored
// //  *  disposition survives exec(), which would break wait() inside the ambient
// //  *  loop's shell and spin it at 100% CPU.  Handlers are reset to SIG_DFL by
// //  *  exec(), so children are unaffected.
// //  * ------------------------------------------------------------------------ */
// // void reapChildren(int)
// // {
// //     while (waitpid(-1, NULL, WNOHANG) > 0) { }
// // }

// // /* --------------------------------------------------------------------------
// //  *  atexit() handlers do NOT run when a process dies from a signal, so a plain
// //  *  `kill <pid>`, a Ctrl-C in the launching terminal, or a closed SSH session
// //  *  used to leave the setsid()-detached bed looping forever - with no window
// //  *  left to close and nothing obvious to kill.
// //  *
// //  *  This handler silences the audio, restores the default disposition and
// //  *  re-raises the same signal, so the process still dies of what killed it and
// //  *  the shell still reports the right status.  Everything it touches
// //  *  (kill/signal/raise) is async-signal-safe.
// //  * ------------------------------------------------------------------------ */
// // void stopAudioAndDie(int sig)
// // {
// //     sceneShutdownAudio();
// //     signal(sig, SIG_DFL);
// //     raise(sig);
// // }

// // /* Shell fragment that plays one file once, on whatever player exists. */
// // std::string playerCommand(const std::string& path)
// // {
// // #if defined(__APPLE__)
// //     return "afplay '" + path + "'";
// // #else
// //     return "if command -v paplay >/dev/null 2>&1; then paplay '" + path +
// //            "'; else aplay -q '" + path + "'; fi";
// // #endif
// // }
// // #endif /* !_WIN32 */

// // } /* anonymous namespace */

// // /* ==========================================================================
// //  *  Audio init / shutdown
// //  * ==========================================================================*/
// // void Scene::initAudio()
// // {
// //     const std::string bed = findAsset(kAmbientName);
// //     if (bed.empty())
// //     {
// //         std::printf("[audio] '%s' not found in assets/ or the working "
// //                     "directory - running without the ambient bed.\n",
// //                     kAmbientName);
// //     }

// // #if !defined(_WIN32)
// //     /* Reap the ambient player if it ever exits on its own. */
// //     signal(SIGCHLD, reapChildren);

// //     /* ...and one covers every way of being killed that atexit() misses. */
// //     signal(SIGINT,  stopAudioAndDie);
// //     signal(SIGTERM, stopAudioAndDie);
// //     signal(SIGHUP,  stopAudioAndDie);
// // #endif

// //     if (bed.empty()) return;

// // #if defined(_WIN32)
// //     /* ---- Windows Multimedia API: looping asynchronous playback --------- */
// //     PlaySound(bed.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
// //     std::printf("[audio] PlaySound loop started (winmm): %s\n", bed.c_str());

// // #else
// //     /* ---- POSIX: detached shell looping the platform's CLI player ------- */
// //     const std::string loop = "while :; do " + playerCommand(bed) + "; done";

// //     gAudioPid = fork();
// //     if (gAudioPid == 0)
// //     {
// //         setsid();                       /* own process group -> killable   */
// //         execlp("/bin/sh", "sh", "-c", loop.c_str(), (char*)NULL);
// //         _exit(127);
// //     }
// //     std::printf("[audio] ambient loop started (pid %d): %s\n",
// //                 static_cast<int>(gAudioPid), bed.c_str());
// // #endif
// // }

// // void sceneShutdownAudio()
// // {
// // #if defined(_WIN32)
// //     PlaySound(NULL, NULL, SND_PURGE);
// // #else
// //     if (gAudioPid > 0)
// //     {
// //         kill(-gAudioPid, SIGTERM);      /* kill the whole process group */
// //         gAudioPid = -1;
// //     }
// // #endif
// // }

// // /* ==========================================================================
// //  *  Construction
// //  * ==========================================================================*/
// // Scene::Scene()
// //     : mTime(0.0f)
// //     , mSunSpin(0.0f)
// //     , mPaused(false)
// //     , mStaticList(0)
// //     , mStaticListValid(false)
// // {
// // }

// // void Scene::init()
// // {
// //     buildPathCenterline();
// //     buildStalls();
// //     buildCustomers();
// //     buildProps();
// //     initAudio();
// // }

// // /* ==========================================================================
// //  *  The winding dirt path
// //  *  --------------------------------------------------------------------------
// //  *  The road enters from the bottom-left foreground, climbs past the left
// //  *  vegetable stall, then swings hard right and runs along the BACK of the
// //  *  three right hand stalls (z ~ -10 .. -20, comfortably behind their rear
// //  *  edges).  It crosses the big tree at (50, -18) and runs all the way out
// //  *  through the RIGHT frame edge.  The same curve feeds the customer road
// //  *  nodes in buildCustomers().
// //  * ==========================================================================*/
// // void Scene::buildPathCenterline()
// // {
// //     Vec3  ctrl[8];
// //     float wid[8];

// //     ctrl[0] = Vec3(-19.0f, 0.0f,  54.0f); wid[0] = 15.0f;
// //     ctrl[1] = Vec3(-12.0f, 0.0f,  40.0f); wid[1] = 13.0f;
// //     ctrl[2] = Vec3( -4.0f, 0.0f,  27.0f); wid[2] = 11.0f;
// //     ctrl[3] = Vec3(  0.0f, 0.0f,  16.0f); wid[3] =  9.4f;
// //     ctrl[4] = Vec3( -1.0f, 0.0f,   2.0f); wid[4] =  8.2f;
// //     ctrl[5] = Vec3(  6.0f, 0.0f, -10.0f); wid[5] =  7.0f;
// //     ctrl[6] = Vec3( 26.0f, 0.0f, -14.0f); wid[6] =  5.8f;
// //     ctrl[7] = Vec3( 86.0f, 0.0f, -22.0f); wid[7] =  5.0f;

// //     mPathPts.clear();
// //     mPathWidth.clear();

// //     const int kSteps = 22;
// //     for (int seg = 0; seg < 7; ++seg)
// //     {
// //         const int i0 = (seg == 0) ? 0 : seg - 1;
// //         const int i1 = seg;
// //         const int i2 = seg + 1;
// //         const int i3 = (seg + 2 > 7) ? 7 : seg + 2;

// //         for (int s = 0; s < kSteps; ++s)
// //         {
// //             const float t = static_cast<float>(s) / kSteps;
// //             mPathPts.push_back(catmullRom(ctrl[i0], ctrl[i1],
// //                                           ctrl[i2], ctrl[i3], t));
// //             mPathWidth.push_back(lerpF(wid[i1], wid[i2], t));
// //         }
// //     }
// //     mPathPts.push_back(ctrl[7]);
// //     mPathWidth.push_back(wid[7]);
// // }

// // /* ==========================================================================
// //  *  Stalls - one market row along the road, every one of them drawn by the
// //  *  single drawGenericStall() renderer; only colour + produce shape differ.
// //  * ==========================================================================*/
// // void Scene::buildStalls()
// // {
// //     /* type, x, z, rotY, canopy colour, owner shirt colour */
// //     struct StallSpec
// //     {
// //         StallType type;
// //         float x, z, rotY;
// //         float cr, cg, cb;
// //         float sr, sg, sb;
// //     };

// //     static const StallSpec kSpecs[] =
// //     {
// //         /* Stall 1 was at z = 26, where its left corner fell outside the fixed
// //          * camera's frustum.  Pushed back along the path (-Z) so the whole
// //          * canopy is on screen. */
// //         { STALL_VEGETABLE, -15.0f, 15.0f,  38.0f,
// //           0.80f, 0.20f, 0.16f,  0.95f, 0.55f, 0.12f },   /* red, carrots   */
// //         { STALL_MELON,      10.0f,  0.0f,  -4.0f,
// //           0.16f, 0.34f, 0.78f,  0.52f, 0.24f, 0.72f },   /* blue, melons   */
// //         { STALL_FRUIT,      20.5f,  2.5f,  -8.0f,
// //           0.94f, 0.76f, 0.14f,  0.30f, 0.68f, 0.24f },   /* yellow, fruit  */
// //         { STALL_TEA,        32.0f,  5.0f, -14.0f,
// //           0.74f, 0.20f, 0.16f,  0.95f, 0.82f, 0.18f },   /* the tea stall  */
// //         { STALL_CRATE,     -14.0f, -2.0f,  70.0f,
// //           0.62f, 0.30f, 0.68f,  0.20f, 0.52f, 0.80f },   /* purple crates  */
// //         { STALL_VEGETABLE, -25.0f,  1.0f,  70.0f,
// //           0.22f, 0.62f, 0.34f,  0.86f, 0.36f, 0.16f },   /* green, carrots */
// //     };
// //     const int kCount = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));

// //     mStalls.clear();
// //     mStalls.resize(static_cast<std::size_t>(kCount));

// //     for (int i = 0; i < kCount; ++i)
// //     {
// //         const StallSpec& s = kSpecs[i];
// //         mStalls[static_cast<std::size_t>(i)].init(
// //             s.type, s.x, s.z, s.rotY,
// //             Color(s.cr, s.cg, s.cb),
// //             Color(s.sr, s.sg, s.sb));
// //     }
// // }

// // /* ==========================================================================
// //  *  Customers - a crowd of ten, generated procedurally.  Each one walks the
// //  *  shared road nodes, detours to the counter of one stall, then rejoins the
// //  *  road and exits.  The waypoint right after the stall-front is the way back
// //  *  to the road, which Customer::arriveAtWaypoint() uses to face the browser
// //  *  at the goods.
// //  * ==========================================================================*/
// // void Scene::buildCustomers()
// // {
// //     /* Shared road nodes: down the left, then a hard right turn that runs
// //      * behind the stall row and off the right edge of the world. */
// //     static const int kNodes = 7;
// //     const Vec3 road[kNodes] =
// //     {
// //         Vec3(-12.0f, 0.0f,  40.0f),
// //         Vec3( -4.0f, 0.0f,  27.0f),
// //         Vec3(  0.0f, 0.0f,  16.0f),
// //         Vec3( -1.0f, 0.0f,   2.0f),
// //         Vec3(  6.0f, 0.0f, -10.0f),
// //         Vec3( 26.0f, 0.0f, -14.0f),
// //         Vec3( 86.0f, 0.0f, -22.0f)    /* exits through the right edge */
// //     };

// //     /* Shirt / trouser palette cycled through the crowd. */
// //     static const float kShirt[10][3] =
// //     {
// //         {0.16f,0.48f,0.86f}, {0.78f,0.16f,0.14f}, {0.88f,0.88f,0.90f},
// //         {0.12f,0.55f,0.52f}, {0.92f,0.62f,0.14f}, {0.55f,0.28f,0.72f},
// //         {0.20f,0.62f,0.26f}, {0.90f,0.42f,0.62f}, {0.36f,0.34f,0.78f},
// //         {0.84f,0.78f,0.28f}
// //     };
// //     static const float kTrouser[4][3] =
// //     {
// //         {0.14f,0.20f,0.42f}, {0.16f,0.17f,0.20f},
// //         {0.34f,0.24f,0.16f}, {0.28f,0.28f,0.32f}
// //     };

// //     const int kCrowd = 10;

// //     mCustomers.clear();
// //     mCustomers.resize(static_cast<std::size_t>(kCrowd));

// //     for (int i = 0; i < kCrowd; ++i)
// //     {
// //         /* Spread the crowd over the stalls. */
// //         const std::size_t si = static_cast<std::size_t>(i) % mStalls.size();
// //         const Vec3 spot = mStalls[si].customerSpot();

// //         /* Leave the road at whichever FRONT node sits closest to that counter.
// //          * We restrict this search to nodes 0..3 because they are in front
// //          * of the stalls. Nodes 4, 5, and 6 run behind the market. If we
// //          * allow a customer to branch from a back node, their straight-line
// //          * path to the counter will pass directly through the solid stall! */
// //         int   nearest = 0;
// //         float best    = 1.0e9f;
// //         for (int n = 0; n <= 3; ++n)
// //         {
// //             const float d = gh::distXZ(road[n], spot);
// //             if (d < best) { best = d; nearest = n; }
// //         }

// //         std::vector<Vec3> p;
// //         /* staggered spawn points up the road, off the top of the world */
// //         p.push_back(Vec3(-18.0f - (i % 4) * 2.4f, 0.0f, 54.0f + (i % 3) * 4.0f));
// //         for (int n = 0; n <= nearest; ++n)
// //             p.push_back(road[n]);

// //         const int shopIdx = static_cast<int>(p.size());
// //         p.push_back(spot);                        /* SHOPPING waypoint */

// //         for (int n = nearest; n < kNodes; ++n)    /* back to the road, then out */
// //             p.push_back(road[n]);

// //         CustomerLook look;
// //         look.shirt    = Color(kShirt[i][0], kShirt[i][1], kShirt[i][2]);
// //         look.trousers = Color(kTrouser[i % 4][0], kTrouser[i % 4][1],
// //                               kTrouser[i % 4][2]);
// //         look.scale    = 0.86f + 0.045f * static_cast<float>(i % 5);

// //         mCustomers[static_cast<std::size_t>(i)].init(
// //             i + 1, p, shopIdx, look,
// //             2.8f + 0.22f * static_cast<float>(i % 5),   /* speed       */
// //             static_cast<float>(i) * 1.6f);              /* spawn delay */
// //     }
// // }

// // /* ==========================================================================
// //  *  Sky props + scattered ground decoration
// //  * ==========================================================================*/
// // void Scene::buildProps()
// // {
// //     /* ---- clouds -------------------------------------------------------- */
// //     mClouds.clear();
// //     const float cx[6] = { -52.0f, -14.0f,  22.0f,  58.0f, -34.0f,  40.0f };
// //     const float cy[6] = {  40.0f,  46.0f,  38.0f,  43.0f,  33.0f,  31.0f };
// //     const float cz[6] = { -52.0f, -50.0f, -54.0f, -48.0f, -46.0f, -44.0f };
// //     const float cs[6] = {   1.30f,  1.55f,  1.10f,  1.35f,  0.90f,  1.00f };

// //     for (int i = 0; i < 6; ++i)
// //     {
// //         Cloud c;
// //         c.x     = cx[i];
// //         c.y     = cy[i];
// //         c.z     = cz[i];
// //         c.scale = cs[i];
// //         c.speed = 0.55f + 0.22f * (i % 3);
// //         c.shape = i % 3;
// //         mClouds.push_back(c);
// //     }

// //     /* ---- birds : 3D sine flight curves -------------------------------- */
// //     mBirds.clear();
// //     for (int i = 0; i < 5; ++i)
// //     {
// //         Bird b;
// //         b.t     = -30.0f - i * 9.0f;
// //         b.speed = 5.4f + 0.7f * i;
// //         b.baseY = 34.0f + 2.4f * i;
// //         b.amp   = 3.2f + 0.8f * i;
// //         b.zPos  = -34.0f - 2.5f * i;
// //         b.flap  = static_cast<float>(i) * 1.1f;
// //         b.scale = 1.15f - 0.08f * i;
// //         mBirds.push_back(b);
// //     }

// //     /* ---- grass tufts : scattered, but never on the dirt path ---------- */
// //     mTufts.clear();
// //     for (int i = 0; i < 220; ++i)
// //     {
// //         const float x = lerpF(-78.0f, 78.0f, hashNoise(i, 11));
// //         const float z = lerpF(-44.0f, 64.0f, hashNoise(i, 23));

// //         /* reject anything close to the path centre line */
// //         bool onPath = false;
// //         for (std::size_t k = 0; k < mPathPts.size(); k += 3)
// //         {
// //             const float dx = mPathPts[k].x - x;
// //             const float dz = mPathPts[k].z - z;
// //             if (dx * dx + dz * dz < (mPathWidth[k] * 0.62f) *
// //                                     (mPathWidth[k] * 0.62f))
// //             { onPath = true; break; }
// //         }
// //         if (!onPath)
// //             mTufts.push_back(Vec3(x, 0.0f, z));
// //     }

// //     /* ---- a few pebbles for foreground interest ------------------------- */
// //     mPebbles.clear();
// //     for (int i = 0; i < 26; ++i)
// //     {
// //         const float x = lerpF(-70.0f, 74.0f, hashNoise(i, 71));
// //         const float z = lerpF(  0.0f, 60.0f, hashNoise(i, 97));
// //         mPebbles.push_back(Vec3(x, 0.0f, z));
// //     }
// // }

// // /* ==========================================================================
// //  *  MASTER UPDATE
// //  * ==========================================================================*/
// // void Scene::updateScene(float dt)
// // {
// //     if (mPaused) return;

// //     mTime    += dt;
// //     mSunSpin += dt * 12.0f;                  /* generic slow sky phase */
// //     if (mSunSpin > 360.0f) mSunSpin -= 360.0f;

// //     /* ---- stalls: smoke particles + shopkeeper idle motion -------------- */
// //     for (std::size_t i = 0; i < mStalls.size(); ++i)
// //         mStalls[i].update(dt);

// //     /* ---- customers: EP3 state machine --------------------------------- */
// //     for (std::size_t i = 0; i < mCustomers.size(); ++i)
// //         mCustomers[i].update(dt);

// //     /* ---- clouds drift right, wrapping around -------------------------- */
// //     for (std::size_t i = 0; i < mClouds.size(); ++i)
// //     {
// //         mClouds[i].x += mClouds[i].speed * dt;
// //         if (mClouds[i].x > 86.0f) mClouds[i].x = -86.0f;
// //     }

// //     /* ---- birds ride their sine curves --------------------------------- */
// //     for (std::size_t i = 0; i < mBirds.size(); ++i)
// //     {
// //         mBirds[i].t    += mBirds[i].speed * dt;
// //         mBirds[i].flap += dt * 9.0f;
// //         if (mBirds[i].t > 78.0f) mBirds[i].t = -78.0f;
// //     }
// // }

// // /* ==========================================================================
// //  *  Camera
// //  * ==========================================================================*/
// // void Scene::applyCamera() const
// // {
// //     /* Single fixed viewpoint: the elevated 3/4 view of colored.jpg.
// //      * No orbit, no zoom - the framing is a constant. */
// //     gluLookAt(4.0, 27.0, 70.0,      /* eye    */
// //               4.0,  6.0, -6.0,      /* target */
// //               0.0,  1.0,  0.0);     /* up     */
// // }

// // /* ==========================================================================
// //  *  MASTER DRAW
// //  * ==========================================================================*/
// // void Scene::drawScene(int screenW, int screenH)
// // {
// //     /* 1. sky gradient (2D overlay, no depth) */
// //     drawSky(screenW, screenH);

// //     /* 2. distant animated sky props */
// //     drawSun();
// //     drawClouds();
// //     drawBirds();

// //     /* 3. cached static world: terrain, path, trees, fences, hedge */
// //     drawStaticWorld();

// //     /* 4. the stalls, each drawing its own vendor + goods */
// //     for (std::size_t i = 0; i < mStalls.size(); ++i)
// //         mStalls[i].draw();

// //     /* 5. the walking customers (EP3 / EP4) */
// //     for (std::size_t i = 0; i < mCustomers.size(); ++i)
// //         mCustomers[i].draw();
// // }

// // void Scene::drawSky(int screenW, int screenH) const
// // {
// //     gh::drawVerticalGradient(screenW, screenH, kSkyTop, kSkyLow);
// // }

// // /* --------------------------------------------------------------------------
// //  *  Static world, compiled into a display list on first use.
// //  * ------------------------------------------------------------------------ */
// // void Scene::drawStaticWorld() const
// // {
// //     if (!mStaticListValid)
// //     {
// //         if (mStaticList == 0)
// //             mStaticList = glGenLists(1);

// //         glNewList(mStaticList, GL_COMPILE_AND_EXECUTE);

// //             drawTerrain();
// //             drawBackgroundHedge();
// //             drawDirtPath();
// //             drawGrassTufts();

// //             /* ---- trees ------------------------------------------------- *
// //              * All of these sit clear of the path centreline: the road swings
// //              * from (26,-14) out to (86,-22), so anything on the right has to
// //              * be pushed well behind that line or it grows out of the dirt. */
// //             drawTree(-46.0f,  -6.0f, 1.20f, 0);       /* big forked, left  */
// //             drawTree(-56.0f, -18.0f, 0.90f, 1);       /* smaller behind it */
// //             drawTree( 52.0f, -28.0f, 1.05f, 0);       /* right, by the tea */
// //             drawTree( 66.0f, -30.0f, 0.80f, 1);

// //             /* A few more large ones filling the middle distance. */
// //             drawTree(-42.0f, -34.0f, 1.30f, 0);
// //             drawTree( -8.0f, -32.0f, 1.20f, 0);
// //             drawTree( 20.0f, -32.0f, 1.25f, 0);
// //             drawTree( 38.0f, -36.0f, 1.15f, 0);

// //             /* ---- the two thatched cottages ----------------------------- */
// //             /* As in the reference: a big cottage with the smaller one right
// //              * beside it, eaves almost touching, the pair sitting behind the
// //              * stall row and left of the path (which has swung to positive x
// //              * by this depth).  Same rotY, so the two ridges stay parallel. */
// //             drawHouse(-32.0f, -14.0f, -8.0f, 1.55f, 2);   /* big, 2 windows */
// //             drawHouse(-16.5f, -18.5f, -8.0f, 1.10f, 1);   /* small, 1 window*/

// //             /* ---- post-and-rail fences ---------------------------------- */
// //             /* The left run sits well back at z = -30/-34 so it reads as a
// //              * field boundary behind the trees rather than beside the path. */
// //             drawFenceRun(Vec3(-82.0f, 0.0f, -30.0f),
// //                          Vec3(-34.0f, 0.0f, -34.0f), 7);   /* left field  */
// //             drawFenceRun(Vec3(  4.0f, 0.0f,  38.0f),
// //                          Vec3( 60.0f, 0.0f,  16.0f), 8);   /* foreground  */
// //             drawFenceRun(Vec3( 60.0f, 0.0f,  16.0f),
// //                          Vec3( 78.0f, 0.0f,  10.0f), 3);

// //         glEndList();
// //         mStaticListValid = true;
// //     }
// //     else
// //     {
// //         glCallList(mStaticList);
// //     }
// // }

// // /* ==========================================================================
// //  *  Terrain - a grid of large grass blocks with subtle per-tile variation
// //  * ==========================================================================*/
// // void Scene::drawTerrain() const
// // {
// //     const bool prevOutline = gh::outlineEnabled();
// //     gh::setOutlineEnabled(false);          /* no grid lines on the ground */

// //     const float tile = 18.0f;
// //     const int   nx   = static_cast<int>((kGroundMaxX - kGroundMinX) / tile) + 1;
// //     const int   nz   = static_cast<int>((kGroundMaxZ - kGroundMinZ) / tile) + 1;

// //     for (int ix = 0; ix < nx; ++ix)
// //         for (int iz = 0; iz < nz; ++iz)
// //         {
// //             const float x = kGroundMinX + tile * (ix + 0.5f);
// //             const float z = kGroundMinZ + tile * (iz + 0.5f);

// //             /* fade a little toward the horizon so depth reads clearly */
// //             const float depthT = std::min(1.0f,
// //                 std::max(0.0f, (z - kGroundMinZ) / (kGroundMaxZ - kGroundMinZ)));
// //             Color c = gh::mixColor(gh::shade(kGrass, 0.93f), kGrass, depthT);

// //             /* tile-to-tile mottling */
// //             c = gh::shade(c, 0.96f + 0.08f * hashNoise(ix, iz));

// //             glPushMatrix();
// //                 glTranslatef(x, -1.0f, z);
// //                 gh::drawBlock(tile, 2.0f, tile, c);
// //             glPopMatrix();
// //         }

// //     /* a darker soil band under the front edge so the ground has thickness */
// //     glPushMatrix();
// //         glTranslatef(0.0f, -1.4f, kGroundMaxZ);
// //         gh::drawBlock(kGroundMaxX - kGroundMinX, 1.6f, 1.2f,
// //                       Color(0.42f, 0.28f, 0.14f));
// //     glPopMatrix();

// //     gh::setOutlineEnabled(prevOutline);
// // }

// // /* ==========================================================================
// //  *  The winding dirt path - flat voxel tiles stamped along the spline
// //  * ==========================================================================*/
// // void Scene::drawDirtPath() const
// // {
// //     if (mPathPts.size() < 2) return;

// //     const bool prevOutline = gh::outlineEnabled();
// //     gh::setOutlineEnabled(false);          /* keep the path smooth */

// //     for (std::size_t i = 0; i + 1 < mPathPts.size(); ++i)
// //     {
// //         const Vec3& a = mPathPts[i];
// //         const Vec3& b = mPathPts[i + 1];

// //         const float dx  = b.x - a.x;
// //         const float dz  = b.z - a.z;
// //         const float len = std::sqrt(dx * dx + dz * dz);
// //         if (len < 1e-4f) continue;

// //         const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);
// //         const float w   = mPathWidth[i];

// //         glPushMatrix();
// //             glTranslatef((a.x + b.x) * 0.5f, 0.0f, (a.z + b.z) * 0.5f);
// //             glRotatef(ang, 0.0f, 1.0f, 0.0f);

// //             /* darker worn edge slightly wider and lower */
// //             glPushMatrix();
// //                 glTranslatef(0.0f, 0.06f, 0.0f);
// //                 gh::drawBlock(w + 1.5f, 0.12f, len * 1.9f, kDirtEdge);
// //             glPopMatrix();

// //             /* main tan surface */
// //             glPushMatrix();
// //                 glTranslatef(0.0f, 0.15f, 0.0f);
// //                 const float mottle = 0.97f + 0.06f * hashNoise(
// //                     static_cast<int>(i), 5);
// //                 gh::drawBlock(w, 0.14f, len * 1.9f,
// //                               gh::shade(kDirtPath, mottle));
// //             glPopMatrix();

// //             /* occasional lighter tread patch down the middle */
// //             if ((i % 7) == 0)
// //             {
// //                 glPushMatrix();
// //                     glTranslatef(hashNoise(static_cast<int>(i), 3) * w * 0.4f -
// //                                  w * 0.2f, 0.23f, 0.0f);
// //                     gh::drawBlock(w * 0.26f, 0.06f, len * 1.6f,
// //                                   gh::shade(kDirtPath, 1.08f));
// //                 glPopMatrix();
// //             }
// //         glPopMatrix();
// //     }

// //     gh::setOutlineEnabled(prevOutline);
// // }

// // /* ==========================================================================
// //  *  Tree : trunk cuboids + a cluster of plain spheres for the canopy.
// //  *  The old voxel-sphere / midpoint-disc rasterisers emitted several hundred
// //  *  shaded cubes per tree; three glutSolidSphere calls read the same.
// //  * ==========================================================================*/
// // void Scene::drawTree(float x, float z, float scale, int variant) const
// // {
// //     /* leaf blobs: x, y, z, radius - relative to the trunk base */
// //     static const float kCanopyA[3][4] = { {-4.4f, 15.0f,  0.6f, 4.6f},
// //                                           { 4.2f, 16.4f, -0.8f, 4.4f},
// //                                           { 0.0f, 19.4f,  0.0f, 4.8f} };
// //     static const float kCanopyB[3][4] = { { 0.0f, 14.0f,  0.0f, 5.0f},
// //                                           {-2.8f, 12.4f,  1.2f, 3.4f},
// //                                           { 2.9f, 12.8f, -1.0f, 3.4f} };

// //     const float (*canopy)[4] = (variant == 0) ? kCanopyA : kCanopyB;
// //     const float trunkH = (variant == 0) ? 12.0f : 10.4f;
// //     const float trunkW = (variant == 0) ?  2.1f :  1.8f;

// //     glPushMatrix();                       /* ---- tree frame ------------- */
// //         glTranslatef(x, 0.0f, z);
// //         glRotatef(hashNoise(static_cast<int>(x), static_cast<int>(z)) * 60.0f,
// //                   0.0f, 1.0f, 0.0f);
// //         glScalef(scale, scale, scale);

// //         /* ---- trunk + root flare ---------------------------------------- */
// //         glPushMatrix();
// //             glTranslatef(0.0f, trunkH * 0.5f, 0.0f);
// //             gh::drawBlock(trunkW, trunkH, trunkW, kTrunk);
// //         glPopMatrix();
// //         glPushMatrix();
// //             glTranslatef(0.0f, 0.50f, 0.0f);
// //             gh::drawBlock(trunkW * 1.42f, 1.00f, trunkW * 1.42f,
// //                           gh::shade(kTrunk, 0.86f));
// //         glPopMatrix();

// //         /* variant 0 forks into two angled branches */
// //         if (variant == 0)
// //         {
// //             const float lean[2] = { -34.0f, 30.0f };
// //             for (int b = 0; b < 2; ++b)
// //             {
// //                 glPushMatrix();
// //                     glTranslatef(0.0f, 9.5f + b, 0.0f);
// //                     glRotatef(lean[b], 0.0f, 0.0f, 1.0f);
// //                     glTranslatef(0.0f, 2.5f, 0.0f);
// //                     gh::drawBlock(1.35f, 5.2f, 1.35f,
// //                                   gh::shade(kTrunk, 1.0f + 0.05f * b));
// //                 glPopMatrix();
// //             }
// //         }

// //         /* ---- sphere canopy -------------------------------------------- */
// //         const float tint[3] = { 1.00f, 1.08f, 0.92f };
// //         for (int i = 0; i < 3; ++i)
// //         {
// //             const Color c = gh::shade((i == 2) ? kLeafDark : kLeafMid, tint[i]);
// //             glColor3f(c.r, c.g, c.b);
// //             glPushMatrix();
// //                 glTranslatef(canopy[i][0], canopy[i][1], canopy[i][2]);
// //                 glutSolidSphere(canopy[i][3], 12, 9);
// //             glPopMatrix();
// //         }

// //     glPopMatrix();                        /* ----------------------------- */
// // }

// // /* ==========================================================================
// //  *  Post-and-rail fence between two world points
// //  * ==========================================================================*/
// // void Scene::drawFenceRun(const Vec3& a, const Vec3& b, int posts) const
// // {
// //     if (posts < 2) posts = 2;

// //     const float dx  = b.x - a.x;
// //     const float dz  = b.z - a.z;
// //     const float len = std::sqrt(dx * dx + dz * dz);
// //     if (len < 1e-3f) return;

// //     const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);

// //     const float postH = 3.4f;
// //     const float span  = len / (posts - 1);

// //     /* ---- posts ---------------------------------------------------------- */
// //     for (int i = 0; i < posts; ++i)
// //     {
// //         const float t = static_cast<float>(i) / (posts - 1);
// //         glPushMatrix();
// //             glTranslatef(a.x + dx * t, postH * 0.5f - 0.2f, a.z + dz * t);
// //             glRotatef(ang, 0.0f, 1.0f, 0.0f);
// //             gh::drawBlock(0.62f, postH, 0.62f, kFenceWood);
// //             /* Rounded cap, rasterised flat by the custom Midpoint Circle
// //              * Algorithm (EP1) - a filled disc of voxels in the XZ plane.
// //              * Cheap here: the whole fence lives in the static display list,
// //              * so these spans are rasterised once and then replayed. */
// //             gh::drawDiscMidpoint3D(0.0f, postH * 0.5f + 0.12f, 0.0f,
// //                                    2, 0.20f, gh::PLANE_XZ,
// //                                    gh::shade(kFenceWood, 1.14f));
// //         glPopMatrix();
// //     }

// //     /* ---- two horizontal rails ------------------------------------------ */
// //     for (int r = 0; r < 2; ++r)
// //     {
// //         const float y = (r == 0) ? 2.35f : 1.30f;
// //         for (int i = 0; i + 1 < posts; ++i)
// //         {
// //             const float t0 = static_cast<float>(i)     / (posts - 1);
// //             const float t1 = static_cast<float>(i + 1) / (posts - 1);
// //             const float mx = a.x + dx * (t0 + t1) * 0.5f;
// //             const float mz = a.z + dz * (t0 + t1) * 0.5f;

// //             glPushMatrix();
// //                 glTranslatef(mx, y, mz);
// //                 glRotatef(ang, 0.0f, 1.0f, 0.0f);
// //                 gh::drawBlock(0.20f, 0.40f, span * 1.02f,
// //                               gh::shade(kFenceWood, (r == 0) ? 1.10f : 0.94f));
// //             glPopMatrix();
// //         }
// //     }
// // }

// // /* ==========================================================================
// //  *  Background hedge - two rows of overlapping green spheres along the horizon:
// //  *  a near row of bushes and a taller, darker row standing in for the far
// //  *  treeline.  The back row used to be a line of cuboids, which read as a row
// //  *  of green boxes on the skyline instead of foliage.
// //  * ==========================================================================*/
// // void Scene::drawBackgroundHedge() const
// // {
// //     for (int i = 0; i < 30; ++i)
// //     {
// //         const float n = hashNoise(i, 41);
// //         const float r = 3.4f + n * 2.2f;
// //         const Color c = gh::shade(kHedge, 0.90f + 0.16f * n);

// //         glColor3f(c.r, c.g, c.b);
// //         glPushMatrix();
// //             glTranslatef(-88.0f + i * 6.1f, r * 0.72f, -46.0f - n * 5.0f);
// //             glutSolidSphere(r, 10, 8);
// //         glPopMatrix();
// //     }

// //     /* far treeline silhouette behind the hedge - bigger, darker, rounder */
// //     for (int i = 0; i < 22; ++i)
// //     {
// //         const float n = hashNoise(i, 67);
// //         const float r = 6.0f + n * 3.4f;
// //         const Color c = gh::shade(kHedge, 0.72f + 0.10f * n);

// //         glColor3f(c.r, c.g, c.b);
// //         glPushMatrix();
// //             glTranslatef(-92.0f + i * 8.4f, r * 0.62f, -56.0f - n * 4.0f);
// //             glutSolidSphere(r, 10, 8);
// //         glPopMatrix();
// //     }
// // }

// // /* ==========================================================================
// //  *  Thatched village house
// //  *  --------------------------------------------------------------------------
// //  *  Modelled on the pair of cottages in reference/colored.jpg: a long cream
// //  *  front wall facing the market (+Z), a steep golden thatch roof whose ridge
// //  *  runs along X so the gable ends face +/-X, and a timber door set left of
// //  *  centre with barred windows filling the wall to its right.
// //  *
// //  *  Authored at "big house" size and uniformly scaled by the caller, so one
// //  *  function serves both cottages.  windows is the number of front windows
// //  *  (2 on the big house, 1 on the narrower one beside it).
// //  * ==========================================================================*/
// // void Scene::drawHouse(float x, float z, float rotY, float scale,
// //                       int windows) const
// // {
// //     /* Footprint is wider than it is deep, so the door wall reads as the long
// //      * face the way it does in the reference. */
// //     const float wallW   = 9.0f;    /* X span                        */
// //     const float wallH   = 4.6f;    /* eaves height above the plinth */
// //     const float wallD   = 6.6f;    /* Z span                        */
// //     const float roofH   = 3.5f;    /* ridge rise above the eaves    */
// //     const float eaves   = 1.25f;   /* thatch overhang on every side */
// //     const float plinthH = 0.40f;
// //     const float front   = wallD * 0.5f;   /* the +Z face we detail */

// //     glPushMatrix();
// //         glTranslatef(x, 0.0f, z);
// //         glRotatef(rotY, 0.0f, 1.0f, 0.0f);
// //         glScalef(scale, scale, scale);

// //         /* ---- grey stone plinth the walls stand on ---------------------- */
// //         glPushMatrix();
// //             glTranslatef(0.0f, plinthH * 0.5f, 0.0f);
// //             gh::drawBlock(wallW + 0.45f, plinthH, wallD + 0.45f, kPlinth);
// //         glPopMatrix();

// //         /* ---- cream rendered walls -------------------------------------- */
// //         glPushMatrix();
// //             glTranslatef(0.0f, plinthH + wallH * 0.5f, 0.0f);
// //             gh::drawBlock(wallW, wallH, wallD, kHouseWall);
// //         glPopMatrix();

// //         /* ---- thatch, ridge along X (gables face +/-X) ------------------- *
// //          * drawRoofPrism is authored with its ridge along Z, so a 90 deg Y
// //          * turn swaps its width/depth into our Z/X - hence the argument
// //          * order below.  The gable colour is the wall in roof shadow, which
// //          * is what gives the reference its grey triangular ends. */
// //         glPushMatrix();
// //             glTranslatef(0.0f, plinthH + wallH, 0.0f);
// //             glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
// //             gh::drawRoofPrism(0.0f, 0.0f, 0.0f,
// //                               wallD + eaves * 2.0f,   /* -> Z span */
// //                               roofH,
// //                               wallW + eaves * 2.0f,   /* -> X span */
// //                               kThatch, gh::shade(kHouseWall, 0.80f));
// //         glPopMatrix();

// //         /* ---- timber door, left of centre, with a stone step ------------- */
// //         const float doorX = -wallW * 0.5f + 1.85f;

// //         glPushMatrix();
// //             glTranslatef(doorX, plinthH + 1.30f, front + 0.02f);
// //             gh::drawBlock(1.50f, 2.60f, 0.16f, kHouseDoor);
// //         glPopMatrix();
// //         glPushMatrix();
// //             glTranslatef(doorX, 0.16f, front + 0.55f);
// //             gh::drawBlock(2.40f, 0.32f, 1.10f, kPlinth);
// //         glPopMatrix();

// //         /* ---- barred windows filling the wall right of the door --------- */
// //         const float winX2[2] = { doorX + 2.55f, doorX + 5.15f };
// //         const float winY     = plinthH + 2.85f;
// //         const int   nWin     = (windows < 1) ? 1 : (windows > 2 ? 2 : windows);

// //         for (int i = 0; i < nWin; ++i)
// //         {
// //             glPushMatrix();
// //                 glTranslatef(winX2[i], winY, front + 0.02f);

// //                 /* Dark glazing in a thin frame.  No mullions: the cottages sit
// //                  * far enough back that window bars only read as noise. */
// //                 gh::drawBlock(1.30f, 1.10f, 0.14f, kHouseTrim);
// //                 glPushMatrix();
// //                     glTranslatef(0.0f, 0.0f, 0.06f);
// //                     gh::drawBlock(1.10f, 0.90f, 0.10f, kWindowDark);
// //                 glPopMatrix();
// //             glPopMatrix();
// //         }

// //     glPopMatrix();
// // }

// // /* ==========================================================================
// //  *  Grass tufts + pebbles
// //  * ==========================================================================*/
// // void Scene::drawGrassTufts() const
// // {
// //     const Color tuft = gh::shade(kGrassDeep, 1.02f);

// //     for (std::size_t i = 0; i < mTufts.size(); ++i)
// //     {
// //         const Vec3& p = mTufts[i];
// //         const float n = hashNoise(static_cast<int>(i), 13);

// //         glPushMatrix();
// //             glTranslatef(p.x, 0.0f, p.z);
// //             glRotatef(n * 90.0f, 0.0f, 1.0f, 0.0f);

// //             /* a small fan of three blades */
// //             for (int b = -1; b <= 1; ++b)
// //             {
// //                 glPushMatrix();
// //                     glTranslatef(b * 0.30f, 0.42f + 0.10f * (1 - std::abs(b)),
// //                                  0.0f);
// //                     glRotatef(b * -14.0f, 0.0f, 0.0f, 1.0f);  /* splay out */
// //                     gh::drawBlock(0.20f, 0.95f + 0.25f * n, 0.20f,
// //                                   gh::shade(tuft, 0.92f + 0.16f * n));
// //                 glPopMatrix();
// //             }
// //         glPopMatrix();
// //     }

// //     for (std::size_t i = 0; i < mPebbles.size(); ++i)
// //     {
// //         const Vec3& p = mPebbles[i];
// //         const float n = hashNoise(static_cast<int>(i), 29);
// //         glPushMatrix();
// //             glTranslatef(p.x, 0.18f, p.z);
// //             gh::drawBlock(0.55f + n * 0.35f, 0.36f, 0.50f + n * 0.30f,
// //                           gh::shade(kStone, 0.90f + 0.2f * n));
// //         glPopMatrix();
// //     }
// // }

// // /* ==========================================================================
// //  *  Sun - EP1 showcase.
// //  *  The disc is rasterised by the custom integer Midpoint Circle Algorithm
// //  *  (drawDiscMidpoint3D) and the eight straight rays by the custom integer
// //  *  Bresenham line algorithm (drawLineBresenham3D), matching the radiating
// //  *  spokes in reference/colored.jpg.  Both emit voxel cubes, so the sun is
// //  *  built from the same blocks as the rest of the scene instead of being a
// //  *  smooth GLU sphere that never quite belonged.
// //  * ==========================================================================*/
// // void Scene::drawSun() const
// // {
// //     /* Rays and disc are chunky on purpose - outlines would fight the shape. */
// //     const bool prevOutline = gh::outlineEnabled();
// //     gh::setOutlineEnabled(false);

// //     const float sx = 46.0f, sy = 30.0f, sz = -52.0f;
// //     const float voxel = 1.05f;
// //     const int   discR = 6;      /* voxels */

// //     glPushMatrix();
// //         glTranslatef(sx, sy, sz);

// //         /* ---- eight radiating rays, Bresenham ------------------------- *
// //          * Drawn first so the disc covers their inner ends.  The mix of
// //          * axis-aligned and diagonal endpoints exercises all three of the
// //          * algorithm's driving-axis branches.                             */
// //         const int kRay[8][2] = { { 13,   0 }, { -13,   0 },
// //                                  {  0,  13 }, {   0, -13 },
// //                                  {  9,   9 }, {  -9,   9 },
// //                                  {  9,  -9 }, {  -9,  -9 } };

// //         for (int i = 0; i < 8; ++i)
// //         {
// //             /* start just outside the disc edge, end at the ray tip */
// //             const int x0 = kRay[i][0] * discR / 13;
// //             const int y0 = kRay[i][1] * discR / 13;

// //             gh::drawLineBresenham3D(0.0f, 0.0f, 0.0f,
// //                                     x0, y0, 0,
// //                                     kRay[i][0], kRay[i][1], 0,
// //                                     voxel, kSunRay);
// //         }

// //         /* ---- the disc itself, midpoint circle ------------------------ *
// //          * Nudged forward in Z so it wins the depth test against the rays. */
// //         gh::drawDiscMidpoint3D(0.0f, 0.0f, 0.6f,
// //                                discR, voxel, gh::PLANE_XY, kSunYellow);
// //     glPopMatrix();

// //     gh::setOutlineEnabled(prevOutline);
// // }

// // /* ==========================================================================
// //  *  Voxel cloud clusters
// //  * ==========================================================================*/
// // void Scene::drawClouds() const
// // {
// //     const bool prevOutline = gh::outlineEnabled();
// //     gh::setOutlineEnabled(false);          /* clouds read better unlined */

// //     for (std::size_t i = 0; i < mClouds.size(); ++i)
// //     {
// //         const Cloud& c = mClouds[i];

// //         glPushMatrix();
// //             glTranslatef(c.x, c.y, c.z);
// //             glScalef(c.scale, c.scale, c.scale);

// //             /* base slab */
// //             gh::drawBlock(13.0f, 2.6f, 5.0f, kCloudWhite);

// //             switch (c.shape)
// //             {
// //                 case 0:
// //                     gh::draw3DCuboid(-2.6f, 1.9f, 0.0f, 6.4f, 2.6f, 4.6f,
// //                                      kCloudWhite.r, kCloudWhite.g,
// //                                      kCloudWhite.b);
// //                     gh::draw3DCuboid( 2.9f, 1.5f, 0.0f, 4.6f, 2.0f, 4.2f,
// //                                      kCloudWhite.r, kCloudWhite.g,
// //                                      kCloudWhite.b);
// //                     break;
// //                 case 1:
// //                     gh::draw3DCuboid( 0.0f, 2.2f, 0.0f, 8.2f, 3.2f, 4.8f,
// //                                      kCloudWhite.r, kCloudWhite.g,
// //                                      kCloudWhite.b);
// //                     gh::draw3DCuboid(-3.4f, 1.4f, 0.0f, 4.4f, 2.0f, 4.2f,
// //                                      kCloudWhite.r, kCloudWhite.g,
// //                                      kCloudWhite.b);
// //                     gh::draw3DCuboid( 3.6f, 1.2f, 0.0f, 4.0f, 1.8f, 4.0f,
// //                                      kCloudWhite.r, kCloudWhite.g,
// //                                      kCloudWhite.b);
// //                     break;
// //                 default:
// //                     gh::draw3DCuboid( 1.0f, 1.7f, 0.0f, 6.0f, 2.4f, 4.4f,
// //                                      kCloudWhite.r, kCloudWhite.g,
// //                                      kCloudWhite.b);
// //                     break;
// //             }
// //         glPopMatrix();
// //     }

// //     gh::setOutlineEnabled(prevOutline);
// // }

// // /* ==========================================================================
// //  *  Birds flying along 3D sine curves, with flapping wings
// //  * ==========================================================================*/
// // void Scene::drawBirds() const
// // {
// //     const Color feather(0.12f, 0.11f, 0.13f);

// //     const bool prevOutline = gh::outlineEnabled();
// //     gh::setOutlineEnabled(false);

// //     for (std::size_t i = 0; i < mBirds.size(); ++i)
// //     {
// //         const Bird& b = mBirds[i];

// //         /* ---- 3D sine flight curve ------------------------------------- */
// //         const float x = b.t;
// //         const float y = b.baseY + b.amp * std::sin(b.t * 0.11f);
// //         const float z = b.zPos  + 7.0f * std::sin(b.t * 0.06f);

// //         const float flap = std::sin(b.flap) * 42.0f;

// //         glPushMatrix();
// //             glTranslatef(x, y, z);
// //             glScalef(b.scale, b.scale, b.scale);
// //             /* bank into the turn */
// //             glRotatef(std::cos(b.t * 0.06f) * 12.0f, 0.0f, 0.0f, 1.0f);

// //             /* body */
// //             gh::drawBlock(1.5f, 0.55f, 0.60f, feather);

// //             /* two flapping wings - pivot at the shoulder (EP4 style) */
// //             for (int s = -1; s <= 1; s += 2)
// //             {
// //                 glPushMatrix();
// //                     glTranslatef(0.0f, 0.12f, s * 0.28f);
// //                     glRotatef(s * flap, 1.0f, 0.0f, 0.0f);
// //                     glTranslatef(0.0f, 0.0f, s * 1.35f);
// //                     gh::drawBlock(1.0f, 0.16f, 2.6f, feather);
// //                 glPopMatrix();
// //             }
// //             /* beak */
// //             glPushMatrix();
// //                 glTranslatef(0.92f, 0.0f, 0.0f);
// //                 gh::drawBlock(0.36f, 0.18f, 0.18f,
// //                               gh::shade(feather, 1.9f));
// //             glPopMatrix();
// //         glPopMatrix();
// //     }

// //     gh::setOutlineEnabled(prevOutline);
// // }

// /* ==========================================================================
//  *  Scene.cpp
//  *  --------------------------------------------------------------------------
//  *  The whole rural market environment, assembled from the voxel primitives in
//  *  GraphicsHelpers, plus the master updateScene() / drawScene() orchestration.
//  * ==========================================================================*/
// #include "Scene.h"
// #include "Customer.h" /* Included to access drawHuman() and CustomerLook */

// #include <algorithm>
// #include <cmath>
// #include <cstdio>
// #include <cstdlib>
// #include <fstream>
// #include <string>

// /* --------------------------------------------------------------------------
//  *  Platform audio headers
//  * ------------------------------------------------------------------------ */
// #if defined(_WIN32)
// #  include <windows.h>
// #  include <mmsystem.h>
// #  ifdef _MSC_VER
// #    pragma comment(lib, "winmm.lib")
// #  endif
// #else
// #  include <signal.h>
// #  include <sys/types.h>
// #  include <sys/wait.h>
// #  include <unistd.h>
// #endif

// using gh::Color;
// using gh::Vec3;

// /* ==========================================================================
//  *  Scene palette - taken straight from the reference artwork
//  * ==========================================================================*/
// namespace {

// const Color kGrass      (0.45f, 0.75f, 0.15f);   /* spec: Minecraft grass  */
// const Color kGrassDeep  (0.34f, 0.60f, 0.12f);
// const Color kDirtPath   (0.85f, 0.70f, 0.35f);   /* spec: tan dirt path    */
// const Color kDirtEdge   (0.70f, 0.55f, 0.26f);
// const Color kStone      (0.58f, 0.58f, 0.57f);   /* pebbles                */
// const Color kFenceWood  (0.48f, 0.31f, 0.16f);
// const Color kTrunk      (0.42f, 0.28f, 0.15f);
// const Color kLeafMid    (0.24f, 0.60f, 0.16f);
// const Color kLeafDark   (0.16f, 0.44f, 0.12f);
// const Color kHedge      (0.19f, 0.47f, 0.13f);

// /* Village houses - cream rendered walls, thatch roof, stone plinth. */
// const Color kHouseWall  (0.91f, 0.88f, 0.78f);   /* cream render          */
// const Color kHouseTrim  (0.55f, 0.52f, 0.46f);   /* gable-end grey band   */
// const Color kThatch     (0.87f, 0.72f, 0.28f);   /* straw roof            */
// const Color kHouseDoor  (0.47f, 0.29f, 0.14f);   /* stained timber door   */
// const Color kWindowDark (0.24f, 0.22f, 0.26f);   /* glazing behind bars   */
// const Color kPlinth     (0.60f, 0.60f, 0.59f);   /* stone base + step     */

// const Color kSunYellow  (1.00f, 0.86f, 0.12f);
// const Color kSunRay     (1.00f, 0.93f, 0.42f);
// const Color kCloudWhite (0.99f, 0.99f, 1.00f);
// const Color kSkyTop     (0.16f, 0.55f, 0.95f);
// const Color kSkyLow     (0.62f, 0.85f, 0.99f);

// /* Terrain extents (world units). */
// const float kGroundMinX = -90.0f;
// const float kGroundMaxX =  90.0f;
// const float kGroundMinZ = -56.0f;
// const float kGroundMaxZ =  70.0f;

// /* --------------------------------------------------------------------------
//  *  Deterministic hash noise - lets the scattered props be regenerated
//  *  identically every frame without storing them (and keeps display lists
//  *  stable).
//  * ------------------------------------------------------------------------ */
// float hashNoise(int a, int b)
// {
//     unsigned int h = static_cast<unsigned int>(a) * 374761393u +
//                      static_cast<unsigned int>(b) * 668265263u;
//     h = (h ^ (h >> 13)) * 1274126177u;
//     h ^= (h >> 16);
//     return static_cast<float>(h % 10000u) / 10000.0f;   /* [0,1) */
// }

// /* Catmull-Rom spline sample - used to build the S-curved dirt path. */
// Vec3 catmullRom(const Vec3& p0, const Vec3& p1,
//                 const Vec3& p2, const Vec3& p3, float t)
// {
//     const float t2 = t * t;
//     const float t3 = t2 * t;

//     Vec3 r;
//     r.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
//                   (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
//                   (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
//     r.y = 0.0f;
//     r.z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
//                   (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
//                   (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);
//     return r;
// }

// float lerpF(float a, float b, float t) { return a + (b - a) * t; }

// /* --------------------------------------------------------------------------
//  *  Kids Playing Football State (Kept in anonymous namespace to avoid header changes)
//  * ------------------------------------------------------------------------ */
// float gFootballT = 0.0f;
// int   gFootballDir = 1;
// float gKidASwing = 0.0f;
// float gKidBSwing = 0.0f;

// const float kKidAX = -50.0f;
// const float kKidAZ =  10.0f;
// const float kKidBX = -50.0f;
// const float kKidBZ =  30.0f;

// void updateKidsAndFootball(float dt) {
//     gFootballT += dt * 1.1f;
//     if (gFootballT > 1.0f) {
//         gFootballT = 0.0f;
//         gFootballDir *= -1; // Switch possession direction
//         if (gFootballDir == 1) gKidASwing = 60.0f; // Kid A kicks
//         else                   gKidBSwing = 60.0f; // Kid B kicks
//     }
    
//     // Limbs relax back to standing
//     gKidASwing += (0.0f - gKidASwing) * std::min(1.0f, dt * 5.0f);
//     gKidBSwing += (0.0f - gKidBSwing) * std::min(1.0f, dt * 5.0f);
// }

// void drawKidsAndFootball() {
//     CustomerLook lookA;
//     lookA.shirt    = Color(0.86f, 0.20f, 0.20f);
//     lookA.trousers = Color(0.14f, 0.20f, 0.42f);
//     lookA.scale    = 0.55f;

//     CustomerLook lookB;
//     lookB.shirt    = Color(0.20f, 0.86f, 0.20f);
//     lookB.trousers = Color(0.34f, 0.24f, 0.16f);
//     lookB.scale    = 0.55f;

//     // Kid A
//     glPushMatrix();
//         glTranslatef(kKidAX, 0.0f, kKidAZ);
//         glRotatef(0.0f, 0.0f, 1.0f, 0.0f); // Face +Z (towards Kid B)
//         glScalef(lookA.scale, lookA.scale, lookA.scale);
//         drawHuman(lookA, 0.0f, gKidASwing, 0.0f);
//     glPopMatrix();

//     // Kid B
//     glPushMatrix();
//         glTranslatef(kKidBX, 0.0f, kKidBZ);
//         glRotatef(180.0f, 0.0f, 1.0f, 0.0f); // Face -Z (towards Kid A)
//         glScalef(lookB.scale, lookB.scale, lookB.scale);
//         drawHuman(lookB, 0.0f, gKidBSwing, 0.0f);
//     glPopMatrix();

//     // Football
//     float startZ = (gFootballDir == 1) ? kKidAZ + 1.0f : kKidBZ - 1.0f;
//     float endZ   = (gFootballDir == 1) ? kKidBZ - 1.0f : kKidAZ + 1.0f;
//     float t = gFootballT;
//     float ballZ = startZ + (endZ - startZ) * t;
//     float ballY = std::sin(t * static_cast<float>(M_PI)) * 2.5f; // Arc height
    
//     glPushMatrix();
//         glTranslatef(kKidAX, ballY + 0.3f, ballZ);
//         // Spin the ball rolling forward
//         glRotatef(t * 720.0f * gFootballDir, 1.0f, 0.0f, 0.0f);
//         gh::drawBlock(0.5f, 0.5f, 0.5f, Color(0.9f, 0.9f, 0.9f));
//         // A patch to break up the color
//         gh::drawBlock(0.55f, 0.2f, 0.2f, Color(0.1f, 0.1f, 0.1f));
//     glPopMatrix();
// }


// /* ==========================================================================
//  *  Audio backend
//  *  --------------------------------------------------------------------------
//  *  One continuous ambient bed (breeze + birdsong) looping forever.  There are
//  *  no farmyard one-shots: the scene has no poultry in it, so a rooster or a
//  *  clucking hen only ever sounded like it came from somewhere else.
//  *
//  *  The WAV lives in assets/ next to the sources, but CMake copies it beside
//  *  the executable too, so both `./rural_market` (repo root) and
//  *  `./build/rural_market` find it - hence the two-place lookup below.
//  * ==========================================================================*/
// const char* kAmbientName = "village_ambient.wav";

// #if !defined(_WIN32)
// pid_t gAudioPid = -1;                /* the looping ambient bed */
// #endif

// bool fileExists(const std::string& path)
// {
//     std::ifstream f(path.c_str(), std::ios::binary);
//     return f.good();
// }

// /* Return "assets/<name>" or "<name>", whichever exists; empty if neither. */
// std::string findAsset(const char* name)
// {
//     std::string candidate = std::string("assets/") + name;
//     if (fileExists(candidate)) return candidate;
//     candidate = name;
//     if (fileExists(candidate)) return candidate;
//     return std::string();
// }

// #if !defined(_WIN32)
// /* --------------------------------------------------------------------------
//  *  Reap finished one-shot players.  Without this every clip would leave a
//  *  zombie behind and a long session would slowly fill the process table -
//  *  the same class of slow leak the HUD string builder used to be.
//  *
//  *  A *handler* is used rather than SIGCHLD = SIG_IGN on purpose: an ignored
//  *  disposition survives exec(), which would break wait() inside the ambient
//  *  loop's shell and spin it at 100% CPU.  Handlers are reset to SIG_DFL by
//  *  exec(), so children are unaffected.
//  * ------------------------------------------------------------------------ */
// void reapChildren(int)
// {
//     while (waitpid(-1, NULL, WNOHANG) > 0) { }
// }

// /* --------------------------------------------------------------------------
//  *  atexit() handlers do NOT run when a process dies from a signal, so a plain
//  *  `kill <pid>`, a Ctrl-C in the launching terminal, or a closed SSH session
//  *  used to leave the setsid()-detached bed looping forever - with no window
//  *  left to close and nothing obvious to kill.
//  *
//  *  This handler silences the audio, restores the default disposition and
//  *  re-raises the same signal, so the process still dies of what killed it and
//  *  the shell still reports the right status.  Everything it touches
//  *  (kill/signal/raise) is async-signal-safe.
//  * ------------------------------------------------------------------------ */
// void stopAudioAndDie(int sig)
// {
//     sceneShutdownAudio();
//     signal(sig, SIG_DFL);
//     raise(sig);
// }

// /* Shell fragment that plays one file once, on whatever player exists. */
// std::string playerCommand(const std::string& path)
// {
// #if defined(__APPLE__)
//     return "afplay '" + path + "'";
// #else
//     return "if command -v paplay >/dev/null 2>&1; then paplay '" + path +
//            "'; else aplay -q '" + path + "'; fi";
// #endif
// }
// #endif /* !_WIN32 */

// } /* anonymous namespace */

// /* ==========================================================================
//  *  Audio init / shutdown
//  * ==========================================================================*/
// void Scene::initAudio()
// {
//     const std::string bed = findAsset(kAmbientName);
//     if (bed.empty())
//     {
//         std::printf("[audio] '%s' not found in assets/ or the working "
//                     "directory - running without the ambient bed.\n",
//                     kAmbientName);
//     }

// #if !defined(_WIN32)
//     /* Reap the ambient player if it ever exits on its own. */
//     signal(SIGCHLD, reapChildren);

//     /* ...and one covers every way of being killed that atexit() misses. */
//     signal(SIGINT,  stopAudioAndDie);
//     signal(SIGTERM, stopAudioAndDie);
//     signal(SIGHUP,  stopAudioAndDie);
// #endif

//     if (bed.empty()) return;

// #if defined(_WIN32)
//     /* ---- Windows Multimedia API: looping asynchronous playback --------- */
//     PlaySound(bed.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
//     std::printf("[audio] PlaySound loop started (winmm): %s\n", bed.c_str());

// #else
//     /* ---- POSIX: detached shell looping the platform's CLI player ------- */
//     const std::string loop = "while :; do " + playerCommand(bed) + "; done";

//     gAudioPid = fork();
//     if (gAudioPid == 0)
//     {
//         setsid();                       /* own process group -> killable   */
//         execlp("/bin/sh", "sh", "-c", loop.c_str(), (char*)NULL);
//         _exit(127);
//     }
//     std::printf("[audio] ambient loop started (pid %d): %s\n",
//                 static_cast<int>(gAudioPid), bed.c_str());
// #endif
// }

// void sceneShutdownAudio()
// {
// #if defined(_WIN32)
//     PlaySound(NULL, NULL, SND_PURGE);
// #else
//     if (gAudioPid > 0)
//     {
//         kill(-gAudioPid, SIGTERM);      /* kill the whole process group */
//         gAudioPid = -1;
//     }
// #endif
// }

// /* ==========================================================================
//  *  Construction
//  * ==========================================================================*/
// Scene::Scene()
//     : mTime(0.0f)
//     , mSunSpin(0.0f)
//     , mPaused(false)
//     , mStaticList(0)
//     , mStaticListValid(false)
// {
// }

// void Scene::init()
// {
//     buildPathCenterline();
//     buildStalls();
//     buildCustomers();
//     buildProps();
//     initAudio();
// }

// /* ==========================================================================
//  *  The winding dirt path
//  *  --------------------------------------------------------------------------
//  *  The road enters from the bottom-left foreground, climbs past the left
//  *  vegetable stall, then swings hard right and runs along the BACK of the
//  *  three right hand stalls (z ~ -10 .. -20, comfortably behind their rear
//  *  edges).  It crosses the big tree at (50, -18) and runs all the way out
//  *  through the RIGHT frame edge.  The same curve feeds the customer road
//  *  nodes in buildCustomers().
//  * ==========================================================================*/
// void Scene::buildPathCenterline()
// {
//     Vec3  ctrl[8];
//     float wid[8];

//     ctrl[0] = Vec3(-19.0f, 0.0f,  54.0f); wid[0] = 15.0f;
//     ctrl[1] = Vec3(-12.0f, 0.0f,  40.0f); wid[1] = 13.0f;
//     ctrl[2] = Vec3( -4.0f, 0.0f,  27.0f); wid[2] = 11.0f;
//     ctrl[3] = Vec3(  0.0f, 0.0f,  16.0f); wid[3] =  9.4f;
//     ctrl[4] = Vec3( -1.0f, 0.0f,   2.0f); wid[4] =  8.2f;
//     ctrl[5] = Vec3(  6.0f, 0.0f, -10.0f); wid[5] =  7.0f;
//     ctrl[6] = Vec3( 26.0f, 0.0f, -14.0f); wid[6] =  5.8f;
//     ctrl[7] = Vec3( 86.0f, 0.0f, -22.0f); wid[7] =  5.0f;

//     mPathPts.clear();
//     mPathWidth.clear();

//     const int kSteps = 22;
//     for (int seg = 0; seg < 7; ++seg)
//     {
//         const int i0 = (seg == 0) ? 0 : seg - 1;
//         const int i1 = seg;
//         const int i2 = seg + 1;
//         const int i3 = (seg + 2 > 7) ? 7 : seg + 2;

//         for (int s = 0; s < kSteps; ++s)
//         {
//             const float t = static_cast<float>(s) / kSteps;
//             mPathPts.push_back(catmullRom(ctrl[i0], ctrl[i1],
//                                           ctrl[i2], ctrl[i3], t));
//             mPathWidth.push_back(lerpF(wid[i1], wid[i2], t));
//         }
//     }
//     mPathPts.push_back(ctrl[7]);
//     mPathWidth.push_back(wid[7]);
// }

// /* ==========================================================================
//  *  Stalls - one market row along the road, every one of them drawn by the
//  *  single drawGenericStall() renderer; only colour + produce shape differ.
//  * ==========================================================================*/
// void Scene::buildStalls()
// {
//     /* type, x, z, rotY, canopy colour, owner shirt colour */
//     struct StallSpec
//     {
//         StallType type;
//         float x, z, rotY;
//         float cr, cg, cb;
//         float sr, sg, sb;
//     };

//     static const StallSpec kSpecs[] =
//     {
//         /* Stall 1 was at z = 26, where its left corner fell outside the fixed
//          * camera's frustum.  Pushed back along the path (-Z) so the whole
//          * canopy is on screen. */
//         { STALL_VEGETABLE, -15.0f, 15.0f,  38.0f,
//           0.80f, 0.20f, 0.16f,  0.95f, 0.55f, 0.12f },   /* red, carrots   */
//         { STALL_MELON,      10.0f,  0.0f,  -4.0f,
//           0.16f, 0.34f, 0.78f,  0.52f, 0.24f, 0.72f },   /* blue, melons   */
//         { STALL_FRUIT,      20.5f,  2.5f,  -8.0f,
//           0.94f, 0.76f, 0.14f,  0.30f, 0.68f, 0.24f },   /* yellow, fruit  */
//         { STALL_TEA,        32.0f,  5.0f, -14.0f,
//           0.74f, 0.20f, 0.16f,  0.95f, 0.82f, 0.18f },   /* the tea stall  */
//         { STALL_CRATE,     -14.0f, -2.0f,  70.0f,
//           0.62f, 0.30f, 0.68f,  0.20f, 0.52f, 0.80f },   /* purple crates  */
//         { STALL_VEGETABLE, -25.0f,  1.0f,  70.0f,
//           0.22f, 0.62f, 0.34f,  0.86f, 0.36f, 0.16f },   /* green, carrots */
//     };
//     const int kCount = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));

//     mStalls.clear();
//     mStalls.resize(static_cast<std::size_t>(kCount));

//     for (int i = 0; i < kCount; ++i)
//     {
//         const StallSpec& s = kSpecs[i];
//         mStalls[static_cast<std::size_t>(i)].init(
//             s.type, s.x, s.z, s.rotY,
//             Color(s.cr, s.cg, s.cb),
//             Color(s.sr, s.sg, s.sb));
//     }
// }

// /* ==========================================================================
//  *  Customers - a crowd of ten, generated procedurally.  Each one walks the
//  *  shared road nodes, detours to the counter of one stall, then rejoins the
//  *  road and exits.  The waypoint right after the stall-front is the way back
//  *  to the road, which Customer::arriveAtWaypoint() uses to face the browser
//  *  at the goods.
//  * ==========================================================================*/
// void Scene::buildCustomers()
// {
//     /* Shared road nodes: down the left, then a hard right turn that runs
//      * behind the stall row and off the right edge of the world. */
//     static const int kNodes = 7;
//     const Vec3 road[kNodes] =
//     {
//         Vec3(-12.0f, 0.0f,  40.0f),
//         Vec3( -4.0f, 0.0f,  27.0f),
//         Vec3(  0.0f, 0.0f,  16.0f),
//         Vec3( -1.0f, 0.0f,   2.0f),
//         Vec3(  6.0f, 0.0f, -10.0f),
//         Vec3( 26.0f, 0.0f, -14.0f),
//         Vec3( 86.0f, 0.0f, -22.0f)    /* exits through the right edge */
//     };

//     /* Shirt / trouser palette cycled through the crowd. */
//     static const float kShirt[10][3] =
//     {
//         {0.16f,0.48f,0.86f}, {0.78f,0.16f,0.14f}, {0.88f,0.88f,0.90f},
//         {0.12f,0.55f,0.52f}, {0.92f,0.62f,0.14f}, {0.55f,0.28f,0.72f},
//         {0.20f,0.62f,0.26f}, {0.90f,0.42f,0.62f}, {0.36f,0.34f,0.78f},
//         {0.84f,0.78f,0.28f}
//     };
//     static const float kTrouser[4][3] =
//     {
//         {0.14f,0.20f,0.42f}, {0.16f,0.17f,0.20f},
//         {0.34f,0.24f,0.16f}, {0.28f,0.28f,0.32f}
//     };

//     const int kCrowd = 10;

//     mCustomers.clear();
//     mCustomers.resize(static_cast<std::size_t>(kCrowd));

//     for (int i = 0; i < kCrowd; ++i)
//     {
//         /* Spread the crowd over the stalls. */
//         const std::size_t si = static_cast<std::size_t>(i) % mStalls.size();
//         const Vec3 spot = mStalls[si].customerSpot();

//         /* Leave the road at whichever FRONT node sits closest to that counter.
//          * We restrict this search to nodes 0..3 because they are in front
//          * of the stalls. Nodes 4, 5, and 6 run behind the market. If we
//          * allow a customer to branch from a back node, their straight-line
//          * path to the counter will pass directly through the solid stall! */
//         int   nearest = 0;
//         float best    = 1.0e9f;
//         for (int n = 0; n <= 3; ++n)
//         {
//             const float d = gh::distXZ(road[n], spot);
//             if (d < best) { best = d; nearest = n; }
//         }

//         std::vector<Vec3> p;
//         /* staggered spawn points up the road, off the top of the world */
//         p.push_back(Vec3(-18.0f - (i % 4) * 2.4f, 0.0f, 54.0f + (i % 3) * 4.0f));
//         for (int n = 0; n <= nearest; ++n)
//             p.push_back(road[n]);

//         const int shopIdx = static_cast<int>(p.size());
//         p.push_back(spot);                        /* SHOPPING waypoint */

//         for (int n = nearest; n < kNodes; ++n)    /* back to the road, then out */
//             p.push_back(road[n]);

//         CustomerLook look;
//         look.shirt    = Color(kShirt[i][0], kShirt[i][1], kShirt[i][2]);
//         look.trousers = Color(kTrouser[i % 4][0], kTrouser[i % 4][1],
//                               kTrouser[i % 4][2]);
//         look.scale    = 0.86f + 0.045f * static_cast<float>(i % 5);

//         mCustomers[static_cast<std::size_t>(i)].init(
//             i + 1, p, shopIdx, look,
//             2.8f + 0.22f * static_cast<float>(i % 5),   /* speed       */
//             static_cast<float>(i) * 1.6f);              /* spawn delay */
//     }
// }

// /* ==========================================================================
//  *  Sky props + scattered ground decoration
//  * ==========================================================================*/
// void Scene::buildProps()
// {
//     /* ---- clouds -------------------------------------------------------- */
//     mClouds.clear();
//     const float cx[6] = { -52.0f, -14.0f,  22.0f,  58.0f, -34.0f,  40.0f };
//     const float cy[6] = {  40.0f,  46.0f,  38.0f,  43.0f,  33.0f,  31.0f };
//     const float cz[6] = { -52.0f, -50.0f, -54.0f, -48.0f, -46.0f, -44.0f };
//     const float cs[6] = {   1.30f,  1.55f,  1.10f,  1.35f,  0.90f,  1.00f };

//     for (int i = 0; i < 6; ++i)
//     {
//         Cloud c;
//         c.x     = cx[i];
//         c.y     = cy[i];
//         c.z     = cz[i];
//         c.scale = cs[i];
//         c.speed = 0.55f + 0.22f * (i % 3);
//         c.shape = i % 3;
//         mClouds.push_back(c);
//     }

//     /* ---- birds : 3D sine flight curves -------------------------------- */
//     mBirds.clear();
//     for (int i = 0; i < 5; ++i)
//     {
//         Bird b;
//         b.t     = -30.0f - i * 9.0f;
//         b.speed = 5.4f + 0.7f * i;
//         b.baseY = 34.0f + 2.4f * i;
//         b.amp   = 3.2f + 0.8f * i;
//         b.zPos  = -34.0f - 2.5f * i;
//         b.flap  = static_cast<float>(i) * 1.1f;
//         b.scale = 1.15f - 0.08f * i;
//         mBirds.push_back(b);
//     }

//     /* ---- grass tufts : scattered, but never on the dirt path ---------- */
//     mTufts.clear();
//     for (int i = 0; i < 220; ++i)
//     {
//         const float x = lerpF(-78.0f, 78.0f, hashNoise(i, 11));
//         const float z = lerpF(-44.0f, 64.0f, hashNoise(i, 23));

//         /* reject anything close to the path centre line */
//         bool onPath = false;
//         for (std::size_t k = 0; k < mPathPts.size(); k += 3)
//         {
//             const float dx = mPathPts[k].x - x;
//             const float dz = mPathPts[k].z - z;
//             if (dx * dx + dz * dz < (mPathWidth[k] * 0.62f) *
//                                     (mPathWidth[k] * 0.62f))
//             { onPath = true; break; }
//         }
//         if (!onPath)
//             mTufts.push_back(Vec3(x, 0.0f, z));
//     }

//     /* ---- a few pebbles for foreground interest ------------------------- */
//     mPebbles.clear();
//     for (int i = 0; i < 26; ++i)
//     {
//         const float x = lerpF(-70.0f, 74.0f, hashNoise(i, 71));
//         const float z = lerpF(  0.0f, 60.0f, hashNoise(i, 97));
//         mPebbles.push_back(Vec3(x, 0.0f, z));
//     }
// }

// /* ==========================================================================
//  *  MASTER UPDATE
//  * ==========================================================================*/
// void Scene::updateScene(float dt)
// {
//     if (mPaused) return;

//     mTime    += dt;
//     mSunSpin += dt * 12.0f;                  /* generic slow sky phase */
//     if (mSunSpin > 360.0f) mSunSpin -= 360.0f;

//     /* ---- stalls: smoke particles + shopkeeper idle motion -------------- */
//     for (std::size_t i = 0; i < mStalls.size(); ++i)
//         mStalls[i].update(dt);

//     /* ---- customers: EP3 state machine --------------------------------- */
//     for (std::size_t i = 0; i < mCustomers.size(); ++i)
//         mCustomers[i].update(dt);

//     /* ---- kids playing football in the corner -------------------------- */
//     updateKidsAndFootball(dt);

//     /* ---- clouds drift right, wrapping around -------------------------- */
//     for (std::size_t i = 0; i < mClouds.size(); ++i)
//     {
//         mClouds[i].x += mClouds[i].speed * dt;
//         if (mClouds[i].x > 86.0f) mClouds[i].x = -86.0f;
//     }

//     /* ---- birds ride their sine curves --------------------------------- */
//     for (std::size_t i = 0; i < mBirds.size(); ++i)
//     {
//         mBirds[i].t    += mBirds[i].speed * dt;
//         mBirds[i].flap += dt * 9.0f;
//         if (mBirds[i].t > 78.0f) mBirds[i].t = -78.0f;
//     }
// }

// /* ==========================================================================
//  *  Camera
//  * ==========================================================================*/
// void Scene::applyCamera() const
// {
//     /* Single fixed viewpoint: the elevated 3/4 view of colored.jpg.
//      * No orbit, no zoom - the framing is a constant. */
//     gluLookAt(4.0, 27.0, 70.0,      /* eye    */
//               4.0,  6.0, -6.0,      /* target */
//               0.0,  1.0,  0.0);     /* up     */
// }

// /* ==========================================================================
//  *  MASTER DRAW
//  * ==========================================================================*/
// void Scene::drawScene(int screenW, int screenH)
// {
//     /* 1. sky gradient (2D overlay, no depth) */
//     drawSky(screenW, screenH);

//     /* 2. distant animated sky props */
//     drawSun();
//     drawClouds();
//     drawBirds();

//     /* 3. cached static world: terrain, path, trees, fences, hedge */
//     drawStaticWorld();

//     /* 4. the stalls, each drawing its own vendor + goods */
//     for (std::size_t i = 0; i < mStalls.size(); ++i)
//         mStalls[i].draw();

//     /* 5. the walking customers (EP3 / EP4) */
//     for (std::size_t i = 0; i < mCustomers.size(); ++i)
//         mCustomers[i].draw();

//     /* 6. Kids playing football */
//     drawKidsAndFootball();
// }

// void Scene::drawSky(int screenW, int screenH) const
// {
//     gh::drawVerticalGradient(screenW, screenH, kSkyTop, kSkyLow);
// }

// /* --------------------------------------------------------------------------
//  *  Static world, compiled into a display list on first use.
//  * ------------------------------------------------------------------------ */
// void Scene::drawStaticWorld() const
// {
//     if (!mStaticListValid)
//     {
//         if (mStaticList == 0)
//             mStaticList = glGenLists(1);

//         glNewList(mStaticList, GL_COMPILE_AND_EXECUTE);

//             drawTerrain();
//             drawBackgroundHedge();
//             drawDirtPath();
//             drawGrassTufts();

//             /* ---- trees ------------------------------------------------- *
//              * All of these sit clear of the path centreline: the road swings
//              * from (26,-14) out to (86,-22), so anything on the right has to
//              * be pushed well behind that line or it grows out of the dirt. */
//             drawTree(-46.0f,  -6.0f, 1.20f, 0);       /* big forked, left  */
//             drawTree(-56.0f, -18.0f, 0.90f, 1);       /* smaller behind it */
//             drawTree( 52.0f, -28.0f, 1.05f, 0);       /* right, by the tea */
//             drawTree( 66.0f, -30.0f, 0.80f, 1);

//             /* A few more large ones filling the middle distance. */
//             drawTree(-42.0f, -34.0f, 1.30f, 0);
//             drawTree( -8.0f, -32.0f, 1.20f, 0);
//             drawTree( 20.0f, -32.0f, 1.25f, 0);
//             drawTree( 38.0f, -36.0f, 1.15f, 0);

//             /* ---- the two thatched cottages ----------------------------- */
//             /* As in the reference: a big cottage with the smaller one right
//              * beside it, eaves almost touching, the pair sitting behind the
//              * stall row and left of the path (which has swung to positive x
//              * by this depth).  Same rotY, so the two ridges stay parallel. */
//             drawHouse(-32.0f, -14.0f, -8.0f, 1.55f, 2);   /* big, 2 windows */
//             drawHouse(-16.5f, -18.5f, -8.0f, 1.10f, 1);   /* small, 1 window*/

//             /* ---- post-and-rail fences ---------------------------------- */
//             /* The left run sits well back at z = -30/-34 so it reads as a
//              * field boundary behind the trees rather than beside the path. */
//             drawFenceRun(Vec3(-82.0f, 0.0f, -30.0f),
//                          Vec3(-34.0f, 0.0f, -34.0f), 7);   /* left field  */
//             drawFenceRun(Vec3(  4.0f, 0.0f,  38.0f),
//                          Vec3( 60.0f, 0.0f,  16.0f), 8);   /* foreground  */
//             drawFenceRun(Vec3( 60.0f, 0.0f,  16.0f),
//                          Vec3( 78.0f, 0.0f,  10.0f), 3);

//         glEndList();
//         mStaticListValid = true;
//     }
//     else
//     {
//         glCallList(mStaticList);
//     }
// }

// /* ==========================================================================
//  *  Terrain - a grid of large grass blocks with subtle per-tile variation
//  * ==========================================================================*/
// void Scene::drawTerrain() const
// {
//     const bool prevOutline = gh::outlineEnabled();
//     gh::setOutlineEnabled(false);          /* no grid lines on the ground */

//     const float tile = 18.0f;
//     const int   nx   = static_cast<int>((kGroundMaxX - kGroundMinX) / tile) + 1;
//     const int   nz   = static_cast<int>((kGroundMaxZ - kGroundMinZ) / tile) + 1;

//     for (int ix = 0; ix < nx; ++ix)
//         for (int iz = 0; iz < nz; ++iz)
//         {
//             const float x = kGroundMinX + tile * (ix + 0.5f);
//             const float z = kGroundMinZ + tile * (iz + 0.5f);

//             /* fade a little toward the horizon so depth reads clearly */
//             const float depthT = std::min(1.0f,
//                 std::max(0.0f, (z - kGroundMinZ) / (kGroundMaxZ - kGroundMinZ)));
//             Color c = gh::mixColor(gh::shade(kGrass, 0.93f), kGrass, depthT);

//             /* tile-to-tile mottling */
//             c = gh::shade(c, 0.96f + 0.08f * hashNoise(ix, iz));

//             glPushMatrix();
//                 glTranslatef(x, -1.0f, z);
//                 gh::drawBlock(tile, 2.0f, tile, c);
//             glPopMatrix();
//         }

//     /* a darker soil band under the front edge so the ground has thickness */
//     glPushMatrix();
//         glTranslatef(0.0f, -1.4f, kGroundMaxZ);
//         gh::drawBlock(kGroundMaxX - kGroundMinX, 1.6f, 1.2f,
//                       Color(0.42f, 0.28f, 0.14f));
//     glPopMatrix();

//     gh::setOutlineEnabled(prevOutline);
// }

// /* ==========================================================================
//  *  The winding dirt path - flat voxel tiles stamped along the spline
//  * ==========================================================================*/
// void Scene::drawDirtPath() const
// {
//     if (mPathPts.size() < 2) return;

//     const bool prevOutline = gh::outlineEnabled();
//     gh::setOutlineEnabled(false);          /* keep the path smooth */

//     for (std::size_t i = 0; i + 1 < mPathPts.size(); ++i)
//     {
//         const Vec3& a = mPathPts[i];
//         const Vec3& b = mPathPts[i + 1];

//         const float dx  = b.x - a.x;
//         const float dz  = b.z - a.z;
//         const float len = std::sqrt(dx * dx + dz * dz);
//         if (len < 1e-4f) continue;

//         const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);
//         const float w   = mPathWidth[i];

//         glPushMatrix();
//             glTranslatef((a.x + b.x) * 0.5f, 0.0f, (a.z + b.z) * 0.5f);
//             glRotatef(ang, 0.0f, 1.0f, 0.0f);

//             /* darker worn edge slightly wider and lower */
//             glPushMatrix();
//                 glTranslatef(0.0f, 0.06f, 0.0f);
//                 gh::drawBlock(w + 1.5f, 0.12f, len * 1.9f, kDirtEdge);
//             glPopMatrix();

//             /* main tan surface */
//             glPushMatrix();
//                 glTranslatef(0.0f, 0.15f, 0.0f);
//                 const float mottle = 0.97f + 0.06f * hashNoise(
//                     static_cast<int>(i), 5);
//                 gh::drawBlock(w, 0.14f, len * 1.9f,
//                               gh::shade(kDirtPath, mottle));
//             glPopMatrix();

//             /* occasional lighter tread patch down the middle */
//             if ((i % 7) == 0)
//             {
//                 glPushMatrix();
//                     glTranslatef(hashNoise(static_cast<int>(i), 3) * w * 0.4f -
//                                  w * 0.2f, 0.23f, 0.0f);
//                     gh::drawBlock(w * 0.26f, 0.06f, len * 1.6f,
//                                   gh::shade(kDirtPath, 1.08f));
//                 glPopMatrix();
//             }
//         glPopMatrix();
//     }

//     gh::setOutlineEnabled(prevOutline);
// }

// /* ==========================================================================
//  *  Tree : trunk cuboids + a cluster of plain spheres for the canopy.
//  *  The old voxel-sphere / midpoint-disc rasterisers emitted several hundred
//  *  shaded cubes per tree; three glutSolidSphere calls read the same.
//  * ==========================================================================*/
// void Scene::drawTree(float x, float z, float scale, int variant) const
// {
//     /* leaf blobs: x, y, z, radius - relative to the trunk base */
//     static const float kCanopyA[3][4] = { {-4.4f, 15.0f,  0.6f, 4.6f},
//                                           { 4.2f, 16.4f, -0.8f, 4.4f},
//                                           { 0.0f, 19.4f,  0.0f, 4.8f} };
//     static const float kCanopyB[3][4] = { { 0.0f, 14.0f,  0.0f, 5.0f},
//                                           {-2.8f, 12.4f,  1.2f, 3.4f},
//                                           { 2.9f, 12.8f, -1.0f, 3.4f} };

//     const float (*canopy)[4] = (variant == 0) ? kCanopyA : kCanopyB;
//     const float trunkH = (variant == 0) ? 12.0f : 10.4f;
//     const float trunkW = (variant == 0) ?  2.1f :  1.8f;

//     glPushMatrix();                       /* ---- tree frame ------------- */
//         glTranslatef(x, 0.0f, z);
//         glRotatef(hashNoise(static_cast<int>(x), static_cast<int>(z)) * 60.0f,
//                   0.0f, 1.0f, 0.0f);
//         glScalef(scale, scale, scale);

//         /* ---- trunk + root flare ---------------------------------------- */
//         glPushMatrix();
//             glTranslatef(0.0f, trunkH * 0.5f, 0.0f);
//             gh::drawBlock(trunkW, trunkH, trunkW, kTrunk);
//         glPopMatrix();
//         glPushMatrix();
//             glTranslatef(0.0f, 0.50f, 0.0f);
//             gh::drawBlock(trunkW * 1.42f, 1.00f, trunkW * 1.42f,
//                           gh::shade(kTrunk, 0.86f));
//         glPopMatrix();

//         /* variant 0 forks into two angled branches */
//         if (variant == 0)
//         {
//             const float lean[2] = { -34.0f, 30.0f };
//             for (int b = 0; b < 2; ++b)
//             {
//                 glPushMatrix();
//                     glTranslatef(0.0f, 9.5f + b, 0.0f);
//                     glRotatef(lean[b], 0.0f, 0.0f, 1.0f);
//                     glTranslatef(0.0f, 2.5f, 0.0f);
//                     gh::drawBlock(1.35f, 5.2f, 1.35f,
//                                   gh::shade(kTrunk, 1.0f + 0.05f * b));
//                 glPopMatrix();
//             }
//         }

//         /* ---- sphere canopy -------------------------------------------- */
//         const float tint[3] = { 1.00f, 1.08f, 0.92f };
//         for (int i = 0; i < 3; ++i)
//         {
//             const Color c = gh::shade((i == 2) ? kLeafDark : kLeafMid, tint[i]);
//             glColor3f(c.r, c.g, c.b);
//             glPushMatrix();
//                 glTranslatef(canopy[i][0], canopy[i][1], canopy[i][2]);
//                 glutSolidSphere(canopy[i][3], 12, 9);
//             glPopMatrix();
//         }

//     glPopMatrix();                        /* ----------------------------- */
// }

// /* ==========================================================================
//  *  Post-and-rail fence between two world points
//  * ==========================================================================*/
// void Scene::drawFenceRun(const Vec3& a, const Vec3& b, int posts) const
// {
//     if (posts < 2) posts = 2;

//     const float dx  = b.x - a.x;
//     const float dz  = b.z - a.z;
//     const float len = std::sqrt(dx * dx + dz * dz);
//     if (len < 1e-3f) return;

//     const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);

//     const float postH = 3.4f;
//     const float span  = len / (posts - 1);

//     /* ---- posts ---------------------------------------------------------- */
//     for (int i = 0; i < posts; ++i)
//     {
//         const float t = static_cast<float>(i) / (posts - 1);
//         glPushMatrix();
//             glTranslatef(a.x + dx * t, postH * 0.5f - 0.2f, a.z + dz * t);
//             glRotatef(ang, 0.0f, 1.0f, 0.0f);
//             gh::drawBlock(0.62f, postH, 0.62f, kFenceWood);
//             /* Rounded cap, rasterised flat by the custom Midpoint Circle
//              * Algorithm (EP1) - a filled disc of voxels in the XZ plane.
//              * Cheap here: the whole fence lives in the static display list,
//              * so these spans are rasterised once and then replayed. */
//             gh::drawDiscMidpoint3D(0.0f, postH * 0.5f + 0.12f, 0.0f,
//                                    2, 0.20f, gh::PLANE_XZ,
//                                    gh::shade(kFenceWood, 1.14f));
//         glPopMatrix();
//     }

//     /* ---- two horizontal rails ------------------------------------------ */
//     for (int r = 0; r < 2; ++r)
//     {
//         const float y = (r == 0) ? 2.35f : 1.30f;
//         for (int i = 0; i + 1 < posts; ++i)
//         {
//             const float t0 = static_cast<float>(i)     / (posts - 1);
//             const float t1 = static_cast<float>(i + 1) / (posts - 1);
//             const float mx = a.x + dx * (t0 + t1) * 0.5f;
//             const float mz = a.z + dz * (t0 + t1) * 0.5f;

//             glPushMatrix();
//                 glTranslatef(mx, y, mz);
//                 glRotatef(ang, 0.0f, 1.0f, 0.0f);
//                 gh::drawBlock(0.20f, 0.40f, span * 1.02f,
//                               gh::shade(kFenceWood, (r == 0) ? 1.10f : 0.94f));
//             glPopMatrix();
//         }
//     }
// }

// /* ==========================================================================
//  *  Background hedge - two rows of overlapping green spheres along the horizon:
//  *  a near row of bushes and a taller, darker row standing in for the far
//  *  treeline.  The back row used to be a line of cuboids, which read as a row
//  *  of green boxes on the skyline instead of foliage.
//  * ==========================================================================*/
// void Scene::drawBackgroundHedge() const
// {
//     for (int i = 0; i < 30; ++i)
//     {
//         const float n = hashNoise(i, 41);
//         const float r = 3.4f + n * 2.2f;
//         const Color c = gh::shade(kHedge, 0.90f + 0.16f * n);

//         glColor3f(c.r, c.g, c.b);
//         glPushMatrix();
//             glTranslatef(-88.0f + i * 6.1f, r * 0.72f, -46.0f - n * 5.0f);
//             glutSolidSphere(r, 10, 8);
//         glPopMatrix();
//     }

//     /* far treeline silhouette behind the hedge - bigger, darker, rounder */
//     for (int i = 0; i < 22; ++i)
//     {
//         const float n = hashNoise(i, 67);
//         const float r = 6.0f + n * 3.4f;
//         const Color c = gh::shade(kHedge, 0.72f + 0.10f * n);

//         glColor3f(c.r, c.g, c.b);
//         glPushMatrix();
//             glTranslatef(-92.0f + i * 8.4f, r * 0.62f, -56.0f - n * 4.0f);
//             glutSolidSphere(r, 10, 8);
//         glPopMatrix();
//     }
// }

// /* ==========================================================================
//  *  Thatched village house
//  * ==========================================================================*/
// void Scene::drawHouse(float x, float z, float rotY, float scale,
//                       int windows) const
// {
//     /* Footprint is wider than it is deep, so the door wall reads as the long
//      * face the way it does in the reference. */
//     const float wallW   = 9.0f;    /* X span                        */
//     const float wallH   = 4.6f;    /* eaves height above the plinth */
//     const float wallD   = 6.6f;    /* Z span                        */
//     const float roofH   = 3.5f;    /* ridge rise above the eaves    */
//     const float eaves   = 1.25f;   /* thatch overhang on every side */
//     const float plinthH = 0.40f;
//     const float front   = wallD * 0.5f;   /* the +Z face we detail */

//     glPushMatrix();
//         glTranslatef(x, 0.0f, z);
//         glRotatef(rotY, 0.0f, 1.0f, 0.0f);
//         glScalef(scale, scale, scale);

//         /* ---- grey stone plinth the walls stand on ---------------------- */
//         glPushMatrix();
//             glTranslatef(0.0f, plinthH * 0.5f, 0.0f);
//             gh::drawBlock(wallW + 0.45f, plinthH, wallD + 0.45f, kPlinth);
//         glPopMatrix();

//         /* ---- cream rendered walls -------------------------------------- */
//         glPushMatrix();
//             glTranslatef(0.0f, plinthH + wallH * 0.5f, 0.0f);
//             gh::drawBlock(wallW, wallH, wallD, kHouseWall);
//         glPopMatrix();

//         /* ---- thatch, ridge along X (gables face +/-X) ------------------- */
//         glPushMatrix();
//             glTranslatef(0.0f, plinthH + wallH, 0.0f);
//             glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
//             gh::drawRoofPrism(0.0f, 0.0f, 0.0f,
//                               wallD + eaves * 2.0f,   /* -> Z span */
//                               roofH,
//                               wallW + eaves * 2.0f,   /* -> X span */
//                               kThatch, gh::shade(kHouseWall, 0.80f));
//         glPopMatrix();

//         /* ---- timber door, left of centre, with a stone step ------------- */
//         const float doorX = -wallW * 0.5f + 1.85f;

//         glPushMatrix();
//             glTranslatef(doorX, plinthH + 1.30f, front + 0.02f);
//             gh::drawBlock(1.50f, 2.60f, 0.16f, kHouseDoor);
//         glPopMatrix();
//         glPushMatrix();
//             glTranslatef(doorX, 0.16f, front + 0.55f);
//             gh::drawBlock(2.40f, 0.32f, 1.10f, kPlinth);
//         glPopMatrix();

//         /* ---- barred windows filling the wall right of the door --------- */
//         const float winX2[2] = { doorX + 2.55f, doorX + 5.15f };
//         const float winY     = plinthH + 2.85f;
//         const int   nWin     = (windows < 1) ? 1 : (windows > 2 ? 2 : windows);

//         for (int i = 0; i < nWin; ++i)
//         {
//             glPushMatrix();
//                 glTranslatef(winX2[i], winY, front + 0.02f);

//                 /* Dark glazing in a thin frame.  No mullions: the cottages sit
//                  * far enough back that window bars only read as noise. */
//                 gh::drawBlock(1.30f, 1.10f, 0.14f, kHouseTrim);
//                 glPushMatrix();
//                     glTranslatef(0.0f, 0.0f, 0.06f);
//                     gh::drawBlock(1.10f, 0.90f, 0.10f, kWindowDark);
//                 glPopMatrix();
//             glPopMatrix();
//         }

//     glPopMatrix();
// }

// /* ==========================================================================
//  *  Grass tufts + pebbles
//  * ==========================================================================*/
// void Scene::drawGrassTufts() const
// {
//     const Color tuft = gh::shade(kGrassDeep, 1.02f);

//     for (std::size_t i = 0; i < mTufts.size(); ++i)
//     {
//         const Vec3& p = mTufts[i];
//         const float n = hashNoise(static_cast<int>(i), 13);

//         glPushMatrix();
//             glTranslatef(p.x, 0.0f, p.z);
//             glRotatef(n * 90.0f, 0.0f, 1.0f, 0.0f);

//             /* a small fan of three blades */
//             for (int b = -1; b <= 1; ++b)
//             {
//                 glPushMatrix();
//                     glTranslatef(b * 0.30f, 0.42f + 0.10f * (1 - std::abs(b)),
//                                  0.0f);
//                     glRotatef(b * -14.0f, 0.0f, 0.0f, 1.0f);  /* splay out */
//                     gh::drawBlock(0.20f, 0.95f + 0.25f * n, 0.20f,
//                                   gh::shade(tuft, 0.92f + 0.16f * n));
//                 glPopMatrix();
//             }
//         glPopMatrix();
//     }

//     for (std::size_t i = 0; i < mPebbles.size(); ++i)
//     {
//         const Vec3& p = mPebbles[i];
//         const float n = hashNoise(static_cast<int>(i), 29);
//         glPushMatrix();
//             glTranslatef(p.x, 0.18f, p.z);
//             gh::drawBlock(0.55f + n * 0.35f, 0.36f, 0.50f + n * 0.30f,
//                           gh::shade(kStone, 0.90f + 0.2f * n));
//         glPopMatrix();
//     }
// }

// /* ==========================================================================
//  *  Sun - EP1 showcase.
//  *  A dynamic voxel sun that slides through the sky. By continuously pushing
//  *  calculated integers to the 3D Bresenham line algorithm based on mSunSpin, 
//  *  the rays rotate while maintaining perfectly grid-aligned voxels!
//  * ==========================================================================*/
// void Scene::drawSun() const
// {
//     /* Rays and disc are chunky on purpose - outlines would fight the shape. */
//     const bool prevOutline = gh::outlineEnabled();
//     gh::setOutlineEnabled(false);

//     // Make the sun slowly arc across the sky driven by mTime
//     const float sx = 46.0f + std::sin(mTime * 0.15f) * 30.0f;
//     const float sy = 38.0f + std::cos(mTime * 0.15f) * 8.0f;
//     const float sz = -52.0f;
//     const float voxel = 1.05f;
//     const int   discR = 6;      /* voxels */

//     glPushMatrix();
//         glTranslatef(sx, sy, sz);

//         /* ---- eight radiating rays, dynamic Bresenham -------------------- */
//         for (int i = 0; i < 8; ++i)
//         {
//             // By resolving rotation mathematically as Bresenham endpoints, we avoid 
//             // the dreaded rotated-diamond look that glRotatef creates for voxels.
//             float angle = (i * 45.0f + mSunSpin) * static_cast<float>(M_PI) / 180.0f;
//             float cosA = std::cos(angle);
//             float sinA = std::sin(angle);
            
//             int rx = static_cast<int>(cosA * 13.0f + (cosA > 0.0f ? 0.5f : -0.5f));
//             int ry = static_cast<int>(sinA * 13.0f + (sinA > 0.0f ? 0.5f : -0.5f));
            
//             int x0 = static_cast<int>(cosA * discR + (cosA > 0.0f ? 0.5f : -0.5f));
//             int y0 = static_cast<int>(sinA * discR + (sinA > 0.0f ? 0.5f : -0.5f));

//             gh::drawLineBresenham3D(0.0f, 0.0f, 0.0f,
//                                     x0, y0, 0,
//                                     rx, ry, 0,
//                                     voxel, kSunRay);
//         }

//         /* ---- the disc itself, midpoint circle ------------------------ *
//          * Nudged forward in Z so it wins the depth test against the rays. */
//         gh::drawDiscMidpoint3D(0.0f, 0.0f, 0.6f,
//                                discR, voxel, gh::PLANE_XY, kSunYellow);
//     glPopMatrix();

//     gh::setOutlineEnabled(prevOutline);
// }

// /* ==========================================================================
//  *  Voxel cloud clusters
//  * ==========================================================================*/
// void Scene::drawClouds() const
// {
//     const bool prevOutline = gh::outlineEnabled();
//     gh::setOutlineEnabled(false);          /* clouds read better unlined */

//     for (std::size_t i = 0; i < mClouds.size(); ++i)
//     {
//         const Cloud& c = mClouds[i];

//         glPushMatrix();
//             glTranslatef(c.x, c.y, c.z);
//             glScalef(c.scale, c.scale, c.scale);

//             /* base slab */
//             gh::drawBlock(13.0f, 2.6f, 5.0f, kCloudWhite);

//             switch (c.shape)
//             {
//                 case 0:
//                     gh::draw3DCuboid(-2.6f, 1.9f, 0.0f, 6.4f, 2.6f, 4.6f,
//                                      kCloudWhite.r, kCloudWhite.g,
//                                      kCloudWhite.b);
//                     gh::draw3DCuboid( 2.9f, 1.5f, 0.0f, 4.6f, 2.0f, 4.2f,
//                                      kCloudWhite.r, kCloudWhite.g,
//                                      kCloudWhite.b);
//                     break;
//                 case 1:
//                     gh::draw3DCuboid( 0.0f, 2.2f, 0.0f, 8.2f, 3.2f, 4.8f,
//                                      kCloudWhite.r, kCloudWhite.g,
//                                      kCloudWhite.b);
//                     gh::draw3DCuboid(-3.4f, 1.4f, 0.0f, 4.4f, 2.0f, 4.2f,
//                                      kCloudWhite.r, kCloudWhite.g,
//                                      kCloudWhite.b);
//                     gh::draw3DCuboid( 3.6f, 1.2f, 0.0f, 4.0f, 1.8f, 4.0f,
//                                      kCloudWhite.r, kCloudWhite.g,
//                                      kCloudWhite.b);
//                     break;
//                 default:
//                     gh::draw3DCuboid( 1.0f, 1.7f, 0.0f, 6.0f, 2.4f, 4.4f,
//                                      kCloudWhite.r, kCloudWhite.g,
//                                      kCloudWhite.b);
//                     break;
//             }
//         glPopMatrix();
//     }

//     gh::setOutlineEnabled(prevOutline);
// }

// /* ==========================================================================
//  *  Birds flying along 3D sine curves, with flapping wings
//  * ==========================================================================*/
// void Scene::drawBirds() const
// {
//     const Color feather(0.12f, 0.11f, 0.13f);

//     const bool prevOutline = gh::outlineEnabled();
//     gh::setOutlineEnabled(false);

//     for (std::size_t i = 0; i < mBirds.size(); ++i)
//     {
//         const Bird& b = mBirds[i];

//         /* ---- 3D sine flight curve ------------------------------------- */
//         const float x = b.t;
//         const float y = b.baseY + b.amp * std::sin(b.t * 0.11f);
//         const float z = b.zPos  + 7.0f * std::sin(b.t * 0.06f);

//         const float flap = std::sin(b.flap) * 42.0f;

//         glPushMatrix();
//             glTranslatef(x, y, z);
//             glScalef(b.scale, b.scale, b.scale);
//             /* bank into the turn */
//             glRotatef(std::cos(b.t * 0.06f) * 12.0f, 0.0f, 0.0f, 1.0f);

//             /* body */
//             gh::drawBlock(1.5f, 0.55f, 0.60f, feather);

//             /* two flapping wings - pivot at the shoulder (EP4 style) */
//             for (int s = -1; s <= 1; s += 2)
//             {
//                 glPushMatrix();
//                     glTranslatef(0.0f, 0.12f, s * 0.28f);
//                     glRotatef(s * flap, 1.0f, 0.0f, 0.0f);
//                     glTranslatef(0.0f, 0.0f, s * 1.35f);
//                     gh::drawBlock(1.0f, 0.16f, 2.6f, feather);
//                 glPopMatrix();
//             }
//             /* beak */
//             glPushMatrix();
//                 glTranslatef(0.92f, 0.0f, 0.0f);
//                 gh::drawBlock(0.36f, 0.18f, 0.18f,
//                               gh::shade(feather, 1.9f));
//             glPopMatrix();
//         glPopMatrix();
//     }

//     gh::setOutlineEnabled(prevOutline);
// }

/* ==========================================================================
 *  Scene.cpp
 *  --------------------------------------------------------------------------
 *  The whole rural market environment, assembled from the voxel primitives in
 *  GraphicsHelpers, plus the master updateScene() / drawScene() orchestration.
 * ==========================================================================*/
#include "Scene.h"
#include "Customer.h" /* Included to access drawHuman() and CustomerLook */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

/* --------------------------------------------------------------------------
 *  Platform audio headers
 * ------------------------------------------------------------------------ */
#if defined(_WIN32)
#  include <windows.h>
#  include <mmsystem.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "winmm.lib")
#  endif
#else
#  include <signal.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

using gh::Color;
using gh::Vec3;

/* ==========================================================================
 *  Scene palette - taken straight from the reference artwork
 * ==========================================================================*/
namespace {

const Color kGrass      (0.45f, 0.75f, 0.15f);   /* spec: Minecraft grass  */
const Color kGrassDeep  (0.34f, 0.60f, 0.12f);
const Color kDirtPath   (0.85f, 0.70f, 0.35f);   /* spec: tan dirt path    */
const Color kDirtEdge   (0.70f, 0.55f, 0.26f);
const Color kStone      (0.58f, 0.58f, 0.57f);   /* pebbles                */
const Color kFenceWood  (0.48f, 0.31f, 0.16f);
const Color kTrunk      (0.42f, 0.28f, 0.15f);
const Color kLeafMid    (0.24f, 0.60f, 0.16f);
const Color kLeafDark   (0.16f, 0.44f, 0.12f);
const Color kHedge      (0.19f, 0.47f, 0.13f);

/* River, banks, bridge and the old boats on the water. */
const Color kWater      (0.20f, 0.48f, 0.72f);   /* flat cartoon blue      */
const Color kWaterDeep  (0.13f, 0.36f, 0.60f);
const Color kWaterFoam  (0.72f, 0.86f, 0.94f);
const Color kRiverBank  (0.56f, 0.44f, 0.24f);   /* wet earth rim          */
const Color kBridgeWood (0.52f, 0.36f, 0.19f);   /* deck planks            */
const Color kBridgeRail (0.44f, 0.29f, 0.15f);
const Color kBoatHull   (0.46f, 0.31f, 0.17f);   /* weathered old timber   */
const Color kBoatTrim   (0.36f, 0.24f, 0.13f);
const Color kBoatInner  (0.57f, 0.42f, 0.25f);

/* Village houses - cream rendered walls, thatch roof, stone plinth. */
const Color kHouseWall  (0.91f, 0.88f, 0.78f);   /* cream render          */
const Color kHouseTrim  (0.55f, 0.52f, 0.46f);   /* gable-end grey band   */
const Color kThatch     (0.87f, 0.72f, 0.28f);   /* straw roof            */
const Color kHouseDoor  (0.47f, 0.29f, 0.14f);   /* stained timber door   */
const Color kWindowDark (0.24f, 0.22f, 0.26f);   /* glazing behind bars   */
const Color kPlinth     (0.60f, 0.60f, 0.59f);   /* stone base + step     */

const Color kSunYellow  (1.00f, 0.86f, 0.12f);
const Color kSunRay     (1.00f, 0.93f, 0.42f);
const Color kCloudWhite (0.99f, 0.99f, 1.00f);
const Color kSkyTop     (0.16f, 0.55f, 0.95f);
const Color kSkyLow     (0.62f, 0.85f, 0.99f);

/* ==========================================================================
 *  Day / night palette
 *  --------------------------------------------------------------------------
 *  Three keyframes for the sky and three for the sunlight tint; the scene
 *  blends between them using daylight() and goldenHour().
 * ==========================================================================*/
const Color kSkyTopNight  (0.02f, 0.04f, 0.14f);
const Color kSkyLowNight  (0.08f, 0.11f, 0.26f);
/* Dusk keyframes.  The zenith colour matters more than the horizon one here:
 * the sky is a full-screen gradient whose bottom edge is hidden behind the
 * terrain and the background hedge, so only the upper band is ever on
 * screen.  A purely horizon-orange sunset would therefore never be seen -
 * hence the warm rose at the top as well as the orange below it. */
const Color kSkyTopDusk   (0.62f, 0.34f, 0.44f);
const Color kSkyLowDusk   (0.99f, 0.56f, 0.24f);

/* The sunlight multiplier at each extreme.  Night is not just "dark": it is
 * cool and blue, which is what actually reads as moonlight next to the warm
 * lamps.  Golden hour runs slightly hot on red so a low sun bleaches. */
const Color kLightDay     (1.00f, 1.00f, 1.00f);
const Color kLightNight   (0.30f, 0.36f, 0.60f);
const Color kLightGolden  (1.16f, 0.84f, 0.60f);

const Color kMoonPale     (0.93f, 0.94f, 0.86f);

/* One full day, in seconds of wall clock. */
const float kDayLengthSec = 150.0f;

/* Height of the sun / moon arc, and how far it swings across the frame.
 * kSkyArcY0 is the height at sunrise / sunset, and it has to clear the
 * background hedge (which tops out near y = 10): if the disc sinks behind
 * the hedge while its Bresenham rays are still long enough to poke over the
 * top, all that is left on screen is a row of stray yellow voxels. */
const float kSkyArcX      = 56.0f;
const float kSkyArcY      = 24.0f;
const float kSkyArcY0     = 13.0f;   /* height at sunrise / sunset          */
const float kSkyArcCx     =  4.0f;   /* the camera's x, so the arc is centred */
const float kSkyArcZ      = -52.0f;

float smoothstep(float e0, float e1, float x)
{
    if (e1 <= e0) return (x < e0) ? 0.0f : 1.0f;
    float t = (x - e0) / (e1 - e0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* Terrain extents (world units). */
const float kGroundMinX = -90.0f;
const float kGroundMaxX =  90.0f;
const float kGroundMinZ = -56.0f;
const float kGroundMaxZ =  70.0f;

/* --------------------------------------------------------------------------
 *  Deterministic hash noise - lets the scattered props be regenerated
 *  identically every frame without storing them (and keeps display lists
 *  stable).
 * ------------------------------------------------------------------------ */
float hashNoise(int a, int b)
{
    unsigned int h = static_cast<unsigned int>(a) * 374761393u +
                     static_cast<unsigned int>(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return static_cast<float>(h % 10000u) / 10000.0f;   /* [0,1) */
}

/* Catmull-Rom spline sample - used to build the S-curved dirt path. */
Vec3 catmullRom(const Vec3& p0, const Vec3& p1,
                const Vec3& p2, const Vec3& p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;

    Vec3 r;
    r.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    r.y = 0.0f;
    r.z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
                  (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
                  (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);
    return r;
}

float lerpF(float a, float b, float t) { return a + (b - a) * t; }

/* ==========================================================================
 *  Pedestrian verge geometry
 *  --------------------------------------------------------------------------
 *  kRoad is the centre line - the lane the horse carts drive down.  Nobody
 *  walks on it: every waypoint handed to a Customer is first pushed sideways
 *  onto one shoulder by vergeNode(), which offsets along the perpendicular of
 *  the direction of travel by half the dirt width plus a margin.
 *
 *  The first and last nodes sit off camera, so the crowd walks in and out of
 *  frame rather than materialising in it.
 * ==========================================================================*/
const int kRoadNodes = 9;

const Vec3 kRoad[kRoadNodes] =
{
    Vec3(-24.0f, 0.0f,  62.0f),   /* off-camera entry                       */
    Vec3(-19.0f, 0.0f,  54.0f),
    Vec3(-12.0f, 0.0f,  40.0f),
    Vec3( -4.0f, 0.0f,  27.0f),
    Vec3(  0.0f, 0.0f,  16.0f),
    Vec3( -1.0f, 0.0f,   2.0f),
    Vec3(  6.0f, 0.0f, -10.0f),
    Vec3( 26.0f, 0.0f, -14.0f),
    Vec3( 86.0f, 0.0f, -22.0f)    /* off-camera exit, right frame edge      */
};

/* Half the drawn dirt width at each node - mirrors the wid[] table in
 * buildPathCenterline(), so a verge walker always clears the road surface. */
const float kRoadHalf[kRoadNodes] =
{ 8.2f, 7.5f, 6.5f, 5.5f, 4.7f, 4.1f, 3.5f, 2.9f, 2.5f };

/* Grass margin beyond the dirt edge.  The verge waypoints are straight-line
 * chords between nodes while the road surface curves, so on the inside of a
 * bend a walker cuts across the dirt unless the margin leaves room for it.
 * 2.6 keeps even the innermost lane off the surface at the tightest corner
 * (clearance there is still +0.35 units). */
const float kVergeGap = 2.6f;

/* Unit vector pointing to a walker's LEFT as they travel node i -> i+1.
 * Facing (dx,dz) with +Y up, the left hand side is (dz,-dx). */
Vec3 roadLeft(int i)
{
    const int a = (i + 1 < kRoadNodes) ? i : i - 1;
    const int b = a + 1;

    const float dx  = kRoad[b].x - kRoad[a].x;
    const float dz  = kRoad[b].z - kRoad[a].z;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0e-4f) return Vec3(1.0f, 0.0f, 0.0f);

    return Vec3(dz / len, 0.0f, -dx / len);
}

/* Node i shifted onto one verge.  side: +1 left, -1 right.  `stagger` spreads
 * individuals across the width of the shoulder so they do not walk in file. */
Vec3 vergeNode(int i, float side, float stagger)
{
    const Vec3  l   = roadLeft(i);
    const float off = (kRoadHalf[i] + kVergeGap + stagger) * side;
    return Vec3(kRoad[i].x + l.x * off, 0.0f, kRoad[i].z + l.z * off);
}

/* Which verge does `spot` stand on, seen from node i?  Used to keep every
 * shopper on the same side as their own stall - nobody crosses the road. */
float sideOfRoad(int i, const Vec3& spot)
{
    const Vec3  l = roadLeft(i);
    const float d = (spot.x - kRoad[i].x) * l.x +
                    (spot.z - kRoad[i].z) * l.z;
    return (d >= 0.0f) ? 1.0f : -1.0f;
}

/* The road node closest to `spot`.  Which side of the road something sits on
 * is only a meaningful question next to the bit of road it sits next to. */
int nearestRoadNode(const Vec3& spot)
{
    int   best = 1;
    float bd   = 1.0e9f;

    for (int i = 1; i < kRoadNodes; ++i)
    {
        const float d = gh::distXZ(kRoad[i], spot);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* --------------------------------------------------------------------------
 *  Which verge is this spot on, asked at the right place.
 *
 *  This used to be sideOfRoad(2, spot) for every stall in the scene - one
 *  fixed node, 40 units up the road from where most of the market actually
 *  is.  The perpendicular at node 2 points north-west, so for a stall far
 *  down the road the test was dominated by the difference in z rather than by
 *  which shoulder the stall stands on, and the melon stall at (10, 0) came
 *  back +1: the west verge, the wrong side of the carriageway entirely.  Its
 *  shoppers walked the west lane and then cut straight over the road to reach
 *  the counter, through whatever traffic happened to be there.
 *
 *  Asking at the NEAREST node is the same question posed where it means
 *  something, and it puts all six stalls on the shoulder they are built on.
 * ------------------------------------------------------------------------ */
float sideOfRoadNearest(const Vec3& spot)
{
    return sideOfRoad(nearestRoadNode(spot), spot);
}

/* ==========================================================================
 *  The river, and the bridge that carries the road over it
 *  --------------------------------------------------------------------------
 *  A second spline, laid out like the road: a Catmull-Rom centre line sampled
 *  into a dense polyline with a per-sample half width, so "is this point in
 *  the water" is one nearest-point query and every consumer - the terrain cut,
 *  the tuft scatter, the hedge, the pedestrian router, the boats - asks the
 *  same table.  The channel can be moved by editing kRiverCtrl alone and
 *  everything that has to keep out of it follows.
 *
 *  The river runs down the right of the frame, out of the treeline at the back
 *  and off the right edge at about z = +4, crossing the road once on its long
 *  eastward leg.  The BRIDGE is not placed by hand: buildRiver() walks the
 *  sampled road and finds the stretch that is actually over water, so the deck
 *  always lands where the crossing really is even if either spline is edited.
 * ==========================================================================*/
const int kRiverCtrlN = 7;

const Vec3 kRiverCtrl[kRiverCtrlN] =
{
    Vec3(44.0f, 0.0f, -52.0f),
    Vec3(45.0f, 0.0f, -40.0f),
    Vec3(45.5f, 0.0f, -28.0f),
    Vec3(45.0f, 0.0f, -16.0f),   /* the road crosses about here            */
    Vec3(47.0f, 0.0f,  -4.0f),
    Vec3(54.0f, 0.0f,   8.0f),
    Vec3(68.0f, 0.0f,  20.0f)    /* off the right frame edge               */
};

/* Half the width of the water at each control point. */
const float kRiverHalfCtrl[kRiverCtrlN] =
{ 6.0f, 6.5f, 7.0f, 7.5f, 8.0f, 8.5f, 9.0f };

/* Bare earth bank drawn outside the water on both sides. */
const float kBankWidth = 2.4f;

/* Sampled channel - filled by buildRiver(). */
std::vector<Vec3>  gRiverPts;
std::vector<float> gRiverHalf;

/* The sampled road, published here by Scene::buildPathCenterline() so the
 * river code can ask about the real drawn road rather than the coarse kRoad
 * polyline it would otherwise have to guess from. */
std::vector<Vec3>  gRoadPts;
std::vector<float> gRoadHalf;

/* The same widths in the form RoadTrack::build() wants (full width, not
 * half), so the swept-envelope measurement can drive a track identical to the
 * one the carriages actually use. */
std::vector<float> gPathWidthForTrack;

/* Per road sample: how far from the centre line a cart body can actually
 * reach there.  Measured by buildSweptEnvelope() rather than assumed - see
 * the note on cartCorridorHalf() for why the obvious formula is wrong. */
std::vector<float> gSweptHalf;

/* ---- the bridge, derived in buildRiver() ------------------------------- */
bool  gBridgeReady = false;
int   gBridgeI0    = -1;      /* first / last gRoadPts index on the deck   */
int   gBridgeI1    = -1;
Vec3  gBridgeMid;             /* deck centre                               */
Vec3  gBridgeDir;             /* unit road direction across the span       */
Vec3  gBridgeLeft;            /* unit road normal, to the left of gBridgeDir */

/* Deck half width and where the footways run.
 *
 * Both are set by what the traffic actually needs rather than by eye.  A cart
 * on this stretch reaches 4.20 from the centre line once its swing through the
 * curve is measured (buildSweptEnvelope), and a walker is 0.55 wide, so the
 * footway cannot sit closer in than 4.75 - at the 5.00 it was first given, the
 * margin was a quarter of a unit.  5.50 leaves three quarters, and the parapet
 * closes the deck 0.75 outside the walkers' shoulders. */
const float kFootwayOff   = 5.50f;
const float kDeckHalfW    = 7.00f;

/* --------------------------------------------------------------------------
 *  Heights.
 *
 *  Nothing in this scene is carved INTO the ground: the grass tiles are solid
 *  blocks whose top face is y = 0, and the dirt road is a thin slab stacked on
 *  top of it at y = 0.22.  Water below y = 0 would simply be buried inside the
 *  grass, so the channel is built the same way the road is - stacked - and the
 *  sunken read comes from the earth bank sitting proud of the water rather
 *  than from any actual excavation.
 * ------------------------------------------------------------------------ */
const float kBankTopY  = 0.13f;   /* bare earth rim, just proud of the water */
const float kWaterTopY = 0.06f;   /* the water itself, below the rim         */
const float kDeckTopY  = 0.19f;   /* bridge deck, level with the dirt road   */

/* Distance from p to the river centre line, and the channel half width
 * there.  Returns false only for an unbuilt river. */
bool riverNearest(const Vec3& p, float& dist, float& halfW)
{
    if (gRiverPts.size() < 2) return false;

    float bestD2 = 1.0e18f;
    float bestH  = gRiverHalf[0];

    for (std::size_t i = 0; i + 1 < gRiverPts.size(); ++i)
    {
        const Vec3& a = gRiverPts[i];
        const Vec3& b = gRiverPts[i + 1];

        const float ex = b.x - a.x;
        const float ez = b.z - a.z;
        const float len2 = ex * ex + ez * ez;
        if (len2 < 1.0e-9f) continue;

        float t = ((p.x - a.x) * ex + (p.z - a.z) * ez) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        const float dx = p.x - (a.x + ex * t);
        const float dz = p.z - (a.z + ez * t);
        const float d2 = dx * dx + dz * dz;

        if (d2 < bestD2)
        {
            bestD2 = d2;
            bestH  = lerpF(gRiverHalf[i], gRiverHalf[i + 1], t);
        }
    }

    dist  = std::sqrt(bestD2);
    halfW = bestH;
    return true;
}

/* Is p in the water?  `margin` widens the test - a positive margin also keeps
 * things off the bank. */
bool inRiver(const Vec3& p, float margin)
{
    float d, h;
    if (!riverNearest(p, d, h)) return false;
    return d < h + margin;
}

/* --------------------------------------------------------------------------
 *  Is p over water with nothing to stand on?
 *
 *  Same question as inRiver(), except that the bridge deck counts as dry land:
 *  the whole point of building it is that the road and the footways either
 *  side of it cross the channel without anybody getting wet.  Path tests use
 *  this; the terrain and prop scatter use the plain inRiver(), because grass
 *  and pebbles have no business on the deck either.
 * ------------------------------------------------------------------------ */
bool onBridgeDeck(const Vec3& p)
{
    if (!gBridgeReady) return false;

    const float dx = p.x - gBridgeMid.x;
    const float dz = p.z - gBridgeMid.z;

    /* into the deck's own frame: along the span, and across it */
    const float along  = dx * gBridgeDir.x  + dz * gBridgeDir.z;
    const float across = dx * gBridgeLeft.x + dz * gBridgeLeft.z;

    const Vec3& a = gRoadPts[static_cast<std::size_t>(gBridgeI0)];
    const Vec3& b = gRoadPts[static_cast<std::size_t>(gBridgeI1)];
    const float halfRun = gh::distXZ(a, b) * 0.5f + 2.0f;

    return std::fabs(along) <= halfRun &&
           std::fabs(across) <= kDeckHalfW;
}

/* In the water AND with no deck underfoot - the test every route has to pass. */
bool inOpenWater(const Vec3& p, float margin)
{
    return inRiver(p, margin) && !onBridgeDeck(p);
}

/* Is any part of the straight leg a->b over open water?
 *
 * Sampled rather than solved: the channel is a curved band, so there is no
 * closed form worth writing here.  The step is a small fraction of the
 * narrowest the channel ever gets (12 units across at its thinnest), so a leg
 * cannot hop over the water between samples. */
bool legInRiver(const Vec3& a, const Vec3& b, float margin)
{
    const float dx  = b.x - a.x;
    const float dz  = b.z - a.z;
    const float len = std::sqrt(dx * dx + dz * dz);

    const int steps = std::max(2, static_cast<int>(len / 0.40f) + 1);
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (inOpenWater(Vec3(a.x + dx * t, 0.0f, a.z + dz * t), margin))
            return true;
    }
    return false;
}

/* Where a walker steps on to / off the bridge, on their own side of the road.
 * `end` is 0 for the near (low index) end of the deck, 1 for the far end. */
Vec3 bridgeFootway(float side, int end){
    if (!gBridgeReady) return gBridgeMid;

    /* A little beyond the deck so the approach leg is already clear of the
     * water before the walker turns on to the crossing. */
    const float reach = 3.0f;
    const float sgn   = (end == 0) ? -1.0f : 1.0f;

    const Vec3& anchor = (end == 0) ? gRoadPts[static_cast<std::size_t>(gBridgeI0)]
                                    : gRoadPts[static_cast<std::size_t>(gBridgeI1)];

    return Vec3(anchor.x + gBridgeDir.x * sgn * reach +
                           gBridgeLeft.x * side * kFootwayOff,
                0.0f,
                anchor.z + gBridgeDir.z * sgn * reach +
                           gBridgeLeft.z * side * kFootwayOff);
}

/* Kept off the crossing: a boat sliding through the bridge piers looks worse
 * than no boat at all, so the stretch of river that runs under the deck is
 * skipped over.  Measured in buildRiver() rather than guessed - the first
 * hand-picked pair of numbers put the far end of the skip directly beneath
 * the footway. */
float gBoatSkipLo = 0.42f;
float gBoatSkipHi = 0.52f;

/* Sample the river spline at t in [0,1]: position and unit direction.  Defined
 * with the boats further down; declared here because buildRiver() uses it to
 * measure which stretch of the channel the bridge deck covers. */
void riverAt(float t, Vec3& pos, Vec3& dir);

/* Build the channel, then find the stretch of road that runs over it. */
void buildRiver()
{
    gRiverPts.clear();
    gRiverHalf.clear();

    const int kSteps = 16;
    for (int seg = 0; seg + 1 < kRiverCtrlN; ++seg)
    {
        const int i0 = (seg == 0) ? 0 : seg - 1;
        const int i1 = seg;
        const int i2 = seg + 1;
        const int i3 = (seg + 2 > kRiverCtrlN - 1) ? kRiverCtrlN - 1 : seg + 2;

        for (int s = 0; s < kSteps; ++s)
        {
            const float t = static_cast<float>(s) / kSteps;
            gRiverPts.push_back(catmullRom(kRiverCtrl[i0], kRiverCtrl[i1],
                                           kRiverCtrl[i2], kRiverCtrl[i3], t));
            gRiverHalf.push_back(lerpF(kRiverHalfCtrl[i1],
                                       kRiverHalfCtrl[i2], t));
        }
    }
    gRiverPts.push_back(kRiverCtrl[kRiverCtrlN - 1]);
    gRiverHalf.push_back(kRiverHalfCtrl[kRiverCtrlN - 1]);

    /* ---- the bridge: whichever run of road samples is over the water ---- *
     * The deck is extended one abutment past the bank at each end so it
     * lands on dry ground rather than stopping at the waterline. */
    gBridgeReady = false;
    gBridgeI0 = gBridgeI1 = -1;

    if (gRoadPts.size() < 2) return;

    for (std::size_t i = 0; i < gRoadPts.size(); ++i)
    {
        if (!inRiver(gRoadPts[i], kBankWidth)) continue;
        if (gBridgeI0 < 0) gBridgeI0 = static_cast<int>(i);
        gBridgeI1 = static_cast<int>(i);
    }

    if (gBridgeI0 < 0) return;                 /* road and river never meet */

    const int last = static_cast<int>(gRoadPts.size()) - 1;
    gBridgeI0 = std::max(0,    gBridgeI0 - 2);
    gBridgeI1 = std::min(last, gBridgeI1 + 2);

    const Vec3& a = gRoadPts[static_cast<std::size_t>(gBridgeI0)];
    const Vec3& b = gRoadPts[static_cast<std::size_t>(gBridgeI1)];

    gBridgeMid = Vec3((a.x + b.x) * 0.5f, 0.0f, (a.z + b.z) * 0.5f);

    const float dx  = b.x - a.x;
    const float dz  = b.z - a.z;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0e-4f) return;

    gBridgeDir  = Vec3(dx / len, 0.0f, dz / len);
    gBridgeLeft = Vec3(gBridgeDir.z, 0.0f, -gBridgeDir.x);
    gBridgeReady = true;

    /* ---- where the river runs under the bridge ------------------------- *
     * Walk the channel and note the span of t whose water is beneath the
     * deck, then pad it by a boat length either side so a hull is clear of
     * the structure before it appears.  Measured rather than guessed: the
     * first hand-picked pair of numbers ended the skip at t = 0.46, which is
     * directly under the footway, and a boat surfaced through the planking. */
    const float halfRun = len * 0.5f + 2.0f;

    float lo = 1.0f, hi = 0.0f;
    for (float t = 0.0f; t <= 1.0f; t += 0.002f)
    {
        Vec3 p, dir;
        riverAt(t, p, dir);

        const float ax = p.x - gBridgeMid.x;
        const float az = p.z - gBridgeMid.z;

        const float along  = ax * gBridgeDir.x  + az * gBridgeDir.z;
        const float across = ax * gBridgeLeft.x + az * gBridgeLeft.z;

        if (std::fabs(along) <= halfRun && std::fabs(across) <= kDeckHalfW)
        {
            if (t < lo) lo = t;
            if (t > hi) hi = t;
        }
    }

    if (lo <= hi)
    {
        const float pad = 0.055f;      /* ~ a boat length of river          */
        gBoatSkipLo = std::max(0.0f, lo - pad);
        gBoatSkipHi = std::min(1.0f, hi + pad);
    }
}

/* ==========================================================================
 *  Keeping people off the carriageway
 *  --------------------------------------------------------------------------
 *  The verge waypoints are laid a fixed margin outside the dirt, but a walker
 *  is also shoved about by the crowd relaxation pass, and a run of shoves in
 *  the same direction can walk somebody off the shoulder and under a cart.
 *  This is the backstop: the corridor a cart body can actually sweep, so a
 *  pedestrian can be pushed straight back out of it.
 * ========================================================================== */
const float kCartHalfBody = 1.70f;   /* widest point of a cart (wheel rim)  */
const float kLaneMinOff   = 2.15f;   /* mirrors RoadTrack::laneOffsetAt     */

/* Distance from p to the road centre line, plus the local dirt half width.
 * `envelope` comes back as the measured swept half width there - how far a
 * cart body can actually get from the centre line at that point. */
bool roadNearest(const Vec3& p, float& dist, float& halfW, float* envelope)
{
    if (gRoadPts.size() < 2) return false;

    float bestD2 = 1.0e18f;
    float bestH  = gRoadHalf[0];
    float bestE  = 0.0f;
    bool  haveE  = (gSweptHalf.size() == gRoadPts.size());

    for (std::size_t i = 0; i + 1 < gRoadPts.size(); ++i)
    {
        const Vec3& a = gRoadPts[i];
        const Vec3& b = gRoadPts[i + 1];

        const float ex = b.x - a.x;
        const float ez = b.z - a.z;
        const float len2 = ex * ex + ez * ez;
        if (len2 < 1.0e-9f) continue;

        float t = ((p.x - a.x) * ex + (p.z - a.z) * ez) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        const float dx = p.x - (a.x + ex * t);
        const float dz = p.z - (a.z + ez * t);
        const float d2 = dx * dx + dz * dz;

        if (d2 < bestD2)
        {
            bestD2 = d2;
            bestH  = lerpF(gRoadHalf[i], gRoadHalf[i + 1], t);
            if (haveE)
                bestE = std::max(gSweptHalf[i], gSweptHalf[i + 1]);
        }
    }

    dist  = std::sqrt(bestD2);
    halfW = bestH;

    if (envelope)
    {
        /* Before the sweep has been measured, fall back to the naive figure
         * so start-up ordering cannot leave this reading zero. */
        *envelope = haveE ? bestE : (bestH * 0.5f + kCartHalfBody);
    }
    return true;
}

/* Convenience overload for the callers that only want the distance. */
bool roadNearest(const Vec3& p, float& dist, float& halfW)
{
    return roadNearest(p, dist, halfW, NULL);
}

/* --------------------------------------------------------------------------
 *  How far from the centre line a cart body can actually reach.
 *
 *  The obvious answer - lane offset plus half a cart - is the right one only
 *  on a straight.  A cart is a RIGID box 13.2 units long (kNoseAhead 8.9 plus
 *  kTailBehind 4.3) riding a curved road, so through a bend its nose and tail
 *  swing wide of the arc its centre follows, exactly as a long vehicle cuts
 *  the corner.  On the sharp turn where the road leaves the market the horse's
 *  head reaches nearly six units past the naive figure.
 *
 *  Trusting the naive figure is what let a shopper stand 4.77 from the centre
 *  line - comfortably "off the road" by that measure - and still be walked
 *  through by a horse, because the body that hit them was 1.7 to the side of a
 *  line that was itself offset into the lane, on a bend.
 *
 *  So the envelope is MEASURED rather than assumed: buildSweptEnvelope() drags
 *  the real box along the real track in both lanes and records, per road
 *  sample, how far out any corner of it ever got, and roadNearest() hands that
 *  figure back.  That keeps the exclusion tight on the straights - where a
 *  blanket six-unit margin would shove the walking lanes into the stalls and
 *  the river - and wide only where carts genuinely swing.
 * ------------------------------------------------------------------------ */

/* Half a body, so a walker's shoulder clears the corridor and not just their
 * centre point. */
const float kWalkerHalf = 0.55f;

/* --------------------------------------------------------------------------
 *  ... except that 0.55 is not half a body, and against the traffic that
 *  matters.  Reading the proportions off drawHuman() in Customer.cpp: the
 *  torso is kTorsoW = 1.15 wide, and each arm hangs outboard of it at
 *  kTorsoW*0.5 + kArmW*0.5, so the outer face of an arm sits 0.935 from the
 *  centre line of the body.  Depth is kTorsoD*0.5 = 0.31.  The avatar turns
 *  about +Y as it walks, so the circle that actually contains it whichever way
 *  it is facing has radius sqrt(0.935^2 + 0.31^2) = 0.985.
 *
 *  Held off the carriageway by 0.55, a villager therefore stood with 0.43 of
 *  shoulder inside the ground a cart sweeps, and the measured worst case was a
 *  body centre 0.608 from a cart's flank - a horse and cart driving through
 *  somebody's arm.  The road clearances below use the real figure.
 *
 *  kWalkerHalf is left alone: it also sets the margins against the stalls and
 *  the river banks, where the routing table (legRoutable) is tuned around it,
 *  and neither of those was ever the complaint.
 * ------------------------------------------------------------------------ */
const float kWalkerBody  = 0.99f;   /* adult, any heading                   */
const float kChildBody   = 0.64f;   /* child: the same, at their 0.58-0.64  */

/* --------------------------------------------------------------------------
 *  Measure the ground a cart body can actually cover.
 *
 *  Drives the real cart box down both lanes in small steps and, for each
 *  corner of it at each step, records against the nearest road sample how far
 *  from the centre line that corner reached.  The result is a per-sample
 *  "nothing may stand within this of the centre line" figure that is tight on
 *  the straights and wide through the bends, without anybody having to guess
 *  which is which.
 *
 *  Called from buildPathCenterline() once the road is sampled.
 * ------------------------------------------------------------------------ */
void buildSweptEnvelope()
{
    gSweptHalf.assign(gRoadPts.size(), 0.0f);
    if (gRoadPts.size() < 2) return;

    RoadTrack track;
    track.build(gRoadPts, gPathWidthForTrack);

    const float total = track.totalLength();
    if (total <= 0.0f) return;

    /* nose and tail, in the cart's own frame */
    const float ends[2] = { Carriage::noseAhead(), -4.30f };

    const int steps = 1200;
    for (int lane = -1; lane <= 1; lane += 2)
        for (int i = 0; i <= steps; ++i)
        {
            const float s = total * static_cast<float>(i) /
                                    static_cast<float>(steps);

            const Vec3  c   = track.positionAt(s);
            const Vec3  n   = track.normalAt(s);
            const float off = track.laneOffsetAt(s) * static_cast<float>(lane);

            const Vec3 mid(c.x + n.x * off, 0.0f, c.z + n.z * off);

            const float h = track.headingAt(s) +
                            ((lane > 0) ? 0.0f : 180.0f);
            const float a = h * 0.01745329f;

            const Vec3 fwd(std::sin(a), 0.0f, std::cos(a));
            const Vec3 rgt(fwd.z, 0.0f, -fwd.x);

            for (int e = 0; e < 2; ++e)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                {
                    const Vec3 corner(
                        mid.x + fwd.x * ends[e] + rgt.x * kCartHalfBody * sgn,
                        0.0f,
                        mid.z + fwd.z * ends[e] + rgt.z * kCartHalfBody * sgn);

                    /* charge this reach to the nearest road sample */
                    std::size_t best = 0;
                    float bestD2 = 1.0e18f;
                    for (std::size_t k = 0; k < gRoadPts.size(); ++k)
                    {
                        const float dx = corner.x - gRoadPts[k].x;
                        const float dz = corner.z - gRoadPts[k].z;
                        const float d2 = dx * dx + dz * dz;
                        if (d2 < bestD2) { bestD2 = d2; best = k; }
                    }

                    /* Skip the two terminal samples.  positionAt() clamps at
                     * the ends of the track, so a cart parked off the end has
                     * its whole 13-unit length projecting past the last point,
                     * and every one of those corners charges its full reach to
                     * that one sample - which reported a 9.7-unit exclusion at
                     * the road's exit and a 10.4 one at its entry, walling off
                     * ground no cart ever touches.  Those samples are off
                     * camera anyway. */
                    if (best == 0 || best + 1 >= gRoadPts.size()) continue;

                    const float reach = std::sqrt(bestD2);
                    if (reach > gSweptHalf[best]) gSweptHalf[best] = reach;
                }
        }

    /* Smooth once so a walker crossing a sample boundary does not step off a
     * cliff in the exclusion width, and floor it at the naive figure. */
    std::vector<float> sm = gSweptHalf;
    for (std::size_t i = 0; i < gSweptHalf.size(); ++i)
    {
        const std::size_t lo = (i > 0) ? i - 1 : 0;
        const std::size_t hi = (i + 1 < gSweptHalf.size()) ? i + 1 : i;

        float v = std::max(gSweptHalf[lo], std::max(gSweptHalf[i],
                                                    gSweptHalf[hi]));
        const float naive = gRoadHalf[i] * 0.5f + kCartHalfBody;
        sm[i] = std::max(v, naive);
    }
    gSweptHalf.swap(sm);
}

/* Is this spot somewhere a cart can run a walker down? */
bool inCartCorridor(const Vec3& p, float extra)
{
    float d, hw, env;
    if (!roadNearest(p, d, hw, &env)) return false;
    return d < env + kWalkerBody + extra;
}

/* --------------------------------------------------------------------------
 *  Nearest dry ground to a point that has ended up in the water.
 *
 *  Straight out along the perpendicular from the river centre line, which for
 *  a channel this shape is also the shortest way to a bank.
 * ------------------------------------------------------------------------ */
Vec3 pushOutOfRiver(const Vec3& p)
{
    if (gRiverPts.size() < 2) return p;

    /* nearest point on the centre line, and the half width there */
    float bestD2 = 1.0e18f, bx = p.x, bz = p.z, bestH = gRiverHalf[0];

    for (std::size_t i = 0; i + 1 < gRiverPts.size(); ++i)
    {
        const Vec3& a = gRiverPts[i];
        const Vec3& b = gRiverPts[i + 1];

        const float ex = b.x - a.x, ez = b.z - a.z;
        const float l2 = ex * ex + ez * ez;
        if (l2 < 1.0e-9f) continue;

        float t = ((p.x - a.x) * ex + (p.z - a.z) * ez) / l2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        const float cx = a.x + ex * t, cz = a.z + ez * t;
        const float dx = p.x - cx,     dz = p.z - cz;
        const float d2 = dx * dx + dz * dz;

        if (d2 < bestD2)
        {
            bestD2 = d2; bx = cx; bz = cz;
            bestH  = lerpF(gRiverHalf[i], gRiverHalf[i + 1], t);
        }
    }

    const float d = std::sqrt(bestD2);
    const float want = bestH + kBankWidth + kWalkerHalf;

    if (d >= want) return p;

    /* Dead on the centre line has no "away" - pick the +X bank arbitrarily. */
    const float nx = (d > 1.0e-3f) ? (p.x - bx) / d : 1.0f;
    const float nz = (d > 1.0e-3f) ? (p.z - bz) / d : 0.0f;

    return Vec3(bx + nx * want, 0.0f, bz + nz * want);
}

/* --------------------------------------------------------------------------
 *  Shove a pedestrian waypoint off the carriageway.
 *
 *  The verge lanes are laid a fixed margin outside the DIRT, but what a walker
 *  actually has to clear is the corridor a cart BODY sweeps, and past the bend
 *  the road narrows faster than the carts do: laneOffsetAt() bottoms out at
 *  2.15, so a cart still needs 3.85 from the centre line where the dirt is
 *  only 2.5 wide.  Anything the router hands back is pushed straight out along
 *  the road normal until it is genuinely clear.
 * ------------------------------------------------------------------------ */
Vec3 keepOffRoad(const Vec3& p)
{
    if (gRoadPts.size() < 2) return p;

    Vec3 out = p;

    /* Two passes: moving the point changes which bit of road is nearest. */
    for (int pass = 0; pass < 2; ++pass)
    {
        float d, hw, env;
        if (!roadNearest(out, d, hw, &env)) break;

        const float want = env + kWalkerBody + 0.35f;
        if (d >= want) break;

        /* Direction away from the centre line.  Exactly on it (d == 0) has no
         * "away", so fall back to the road's own left normal there. */
        float nx, nz;
        if (d > 1.0e-3f)
        {
            /* recover the nearest centre-line point from the same walk */
            float bestD2 = 1.0e18f, bx = out.x, bz = out.z;
            for (std::size_t i = 0; i + 1 < gRoadPts.size(); ++i)
            {
                const Vec3& a = gRoadPts[i];
                const Vec3& b = gRoadPts[i + 1];
                const float ex = b.x - a.x, ez = b.z - a.z;
                const float l2 = ex * ex + ez * ez;
                if (l2 < 1.0e-9f) continue;
                float t = ((out.x - a.x) * ex + (out.z - a.z) * ez) / l2;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                const float cx = a.x + ex * t, cz = a.z + ez * t;
                const float dx = out.x - cx, dz = out.z - cz;
                const float d2 = dx * dx + dz * dz;
                if (d2 < bestD2) { bestD2 = d2; bx = cx; bz = cz; }
            }
            nx = (out.x - bx) / d;
            nz = (out.z - bz) / d;
        }
        else
        {
            const Vec3 l = roadLeft(nearestRoadNode(out));
            nx = l.x; nz = l.z;
        }

        out.x += nx * (want - d);
        out.z += nz * (want - d);
    }
    return out;
}

/* ==========================================================================
 *  Stall footprints - the solid volumes people have to walk AROUND
 *  --------------------------------------------------------------------------
 *  Customers used to walk a straight chord from the road verge to the counter
 *  they wanted.  Nothing tested that chord, so a shopper heading for one of
 *  the back-left stalls walked clean through the canopy of the stall in front
 *  of it - the "ghosting" bug.
 *
 *  The fix needs one honest question answered before a path is built: does
 *  this straight leg pass through a stall?  Every stall is an oriented box,
 *  so the test is a segment/box intersection done in the stall's own local
 *  frame, where the box is axis aligned and the test is exact slab clipping.
 *  No sampling, so a clipped corner cannot slip through.
 * ==========================================================================*/

/* The canopy is the widest part of a stall (drawGenericStall builds it at
 * kStallW + 1.4 by kStallD + 1.6).  Half a body width is added so people
 * clear the posts instead of brushing them. */
const float kStallW      = 9.0f;
const float kStallD      = 4.4f;
const float kBodyMargin  = 0.60f;
const float kStallHalfW  = (kStallW + 1.4f) * 0.5f + kBodyMargin;
const float kStallHalfD  = (kStallD + 1.6f) * 0.5f + kBodyMargin;

/* One shared placement table.  buildStalls() draws from it and the path
 * planner tests against it, so the geometry can never drift apart. */
struct StallSpec
{
    StallType type;
    float     x, z, rotY;
    float     cr, cg, cb;      /* canopy      */
    float     sr, sg, sb;      /* owner shirt */
};

const StallSpec kStallSpecs[] =
{
    /* Stall 1 was at z = 26, where its left corner fell outside the fixed
     * camera's frustum.  Pushed back along the path (-Z) so the whole
     * canopy is on screen. */
    { STALL_VEGETABLE, -15.0f, 15.0f,  38.0f,
      0.80f, 0.20f, 0.16f,  0.95f, 0.55f, 0.12f },   /* red, carrots   */
    { STALL_MELON,      10.0f,  0.0f,  -4.0f,
      0.16f, 0.34f, 0.78f,  0.52f, 0.24f, 0.72f },   /* blue, melons   */
    { STALL_FRUIT,      20.5f,  2.5f,  -8.0f,
      0.94f, 0.76f, 0.14f,  0.30f, 0.68f, 0.24f },   /* yellow, fruit  */
    { STALL_TEA,        32.0f,  5.0f, -14.0f,
      0.74f, 0.20f, 0.16f,  0.95f, 0.82f, 0.18f },   /* the tea stall  */
    { STALL_CRATE,     -14.0f, -2.0f,  70.0f,
      0.62f, 0.30f, 0.68f,  0.20f, 0.52f, 0.80f },   /* purple crates  */
    { STALL_VEGETABLE, -25.0f,  1.0f,  70.0f,
      0.22f, 0.62f, 0.34f,  0.86f, 0.36f, 0.16f }    /* green, carrots */
};
const int kStallCount =
    static_cast<int>(sizeof(kStallSpecs) / sizeof(kStallSpecs[0]));

/* World -> one stall's local frame.  glRotatef(rotY, 0,1,0) sends local +X to
 * (cos a, -sin a) and local +Z to (sin a, cos a), so the inverse is this. */
void toStallLocal(const StallSpec& s, const Vec3& p, float& lx, float& lz)
{
    const float a  = s.rotY * 0.01745329f;
    const float c  = std::cos(a);
    const float sn = std::sin(a);
    const float dx = p.x - s.x;
    const float dz = p.z - s.z;
    lx = dx * c  - dz * sn;
    lz = dx * sn + dz * c;
}

/* Local +Z is the way a stall faces, i.e. out over its counter. */
Vec3 stallFrontDir(const StallSpec& s)
{
    const float a = s.rotY * 0.01745329f;
    return Vec3(std::sin(a), 0.0f, std::cos(a));
}

bool pointInAnyStall(const Vec3& p)
{
    for (int i = 0; i < kStallCount; ++i)
    {
        float lx, lz;
        toStallLocal(kStallSpecs[i], p, lx, lz);
        if (std::fabs(lx) <= kStallHalfW && std::fabs(lz) <= kStallHalfD)
            return true;
    }
    return false;
}

/* Exact 2D segment vs axis-aligned box (Liang-Barsky slab clipping), run in
 * the stall's local frame.  Returns true when the segment touches the box. */
bool segHitsBox(float ax, float az, float bx, float bz,
                float hw, float hd)
{
    const float d[2] = { bx - ax, bz - az };
    const float o[2] = { ax, az };
    const float h[2] = { hw, hd };

    float t0 = 0.0f, t1 = 1.0f;
    for (int k = 0; k < 2; ++k)
    {
        if (std::fabs(d[k]) < 1.0e-6f)
        {
            if (o[k] < -h[k] || o[k] > h[k]) return false;  /* parallel, outside */
            continue;
        }
        float ta = (-h[k] - o[k]) / d[k];
        float tb = ( h[k] - o[k]) / d[k];
        if (ta > tb) { const float sw = ta; ta = tb; tb = sw; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1) return false;
    }
    return true;
}

/* Can somebody walk straight from a to b without entering a stall? */
bool legClearOfStalls(const Vec3& a, const Vec3& b)
{
    for (int i = 0; i < kStallCount; ++i)
    {
        float ax, az, bx, bz;
        toStallLocal(kStallSpecs[i], a, ax, az);
        toStallLocal(kStallSpecs[i], b, bx, bz);
        if (segHitsBox(ax, az, bx, bz, kStallHalfW, kStallHalfD))
            return false;
    }
    return true;
}

/* ==========================================================================
 *  The post-and-rail fences, in one table
 *  --------------------------------------------------------------------------
 *  These used to be three literals buried in the display-list build, which made
 *  them invisible to everything except the renderer.  Nothing that laid out a
 *  walking lane knew a fence existed, so the lanes were routed through the
 *  timber: the market-side crowd walked clean through the rails of the near-bank
 *  run, and the west verge lane passed within 0.30 units of that run's first
 *  post - close enough for a villager's shoulder to go through it.
 *
 *  Now the renderer and the pedestrian router read the same table.
 *
 *  gateT0/gateT1 mark one bay left open as a field gate, as a fraction along
 *  the run; a run with gateT1 <= gateT0 is unbroken.  The opening is what lets
 *  the market-side crowd off the frontage without walking through a fence, and
 *  it is placed on the run's OWN post positions so every post keeps its place
 *  and its spacing, with one either side of the gap as the gateposts.
 * ==========================================================================*/
struct FenceRun
{
    Vec3  a, b;
    int   posts;
    float gateT0, gateT1;
};

const int kFenceCount = 3;

const FenceRun kFences[kFenceCount] =
{
    /* Left field boundary, well back at z = -30/-34 so it reads as a field
     * edge behind the trees rather than beside the path. */
    { Vec3(-82.0f, 0.0f, -30.0f), Vec3(-34.0f, 0.0f, -34.0f), 7, 0.0f, 0.0f },

    /* Foreground boundary.  It used to carry on to (78,10), straight through
     * the river; it now stops on the near bank, which is what a field boundary
     * meeting a watercourse actually does.  The market-side walking lane
     * crosses it at about (37.4, 24.6), so the bay that lane passes through -
     * bay 5 of 6, t = 4/6 .. 5/6 - is the gate. */
    { Vec3(  4.0f, 0.0f,  38.0f), Vec3( 49.0f, 0.0f,  20.0f), 7,
      4.0f / 6.0f, 5.0f / 6.0f },

    /* And the boundary picking up again on the far bank. */
    { Vec3( 68.0f, 0.0f,   2.0f), Vec3( 82.0f, 0.0f,  -2.0f), 3, 0.0f, 0.0f },
};

/* Post i of a run, in world space. */
Vec3 fencePostAt(const FenceRun& r, int i)
{
    const float t = static_cast<float>(i) /
                    static_cast<float>((r.posts > 1) ? r.posts - 1 : 1);
    return Vec3(r.a.x + (r.b.x - r.a.x) * t, 0.0f,
                r.a.z + (r.b.z - r.a.z) * t);
}

/* How much room a body needs to clear the timber: the widest part of the drawn
 * avatar plus half a post (they are 0.62 square), and a little daylight so it
 * reads as walking past a fence rather than brushing it. */
const float kFenceClear = kWalkerBody + 0.31f + 0.15f;

/* Shortest distance between segments a0-a1 and b0-b1, in the XZ plane.  Sampled
 * rather than solved: the fences are a handful of fixed runs tested only while
 * the paths are being built, so 24 steps is both plenty and free. */
float segSegDistXZ(const Vec3& a0, const Vec3& a1,
                   const Vec3& b0, const Vec3& b1)
{
    float best = 1.0e9f;

    for (int s = 0; s <= 24; ++s)
    {
        const float t = static_cast<float>(s) / 24.0f;
        const Vec3  p(a0.x + (a1.x - a0.x) * t, 0.0f,
                      a0.z + (a1.z - a0.z) * t);

        /* nearest point of b to p */
        const float ex = b1.x - b0.x;
        const float ez = b1.z - b0.z;
        const float l2 = ex * ex + ez * ez;

        float u = (l2 > 1.0e-9f)
                    ? ((p.x - b0.x) * ex + (p.z - b0.z) * ez) / l2 : 0.0f;
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;

        const float dx = p.x - (b0.x + ex * u);
        const float dz = p.z - (b0.z + ez * u);
        const float d  = std::sqrt(dx * dx + dz * dz);
        if (d < best) best = d;
    }
    return best;
}

/* Can somebody walk straight from a to b without going through the timber?
 * An open gate bay is not timber, so a run with a gate is tested as its two
 * standing pieces. */
bool legClearOfFences(const Vec3& a, const Vec3& b)
{
    for (int i = 0; i < kFenceCount; ++i)
    {
        const FenceRun& r = kFences[i];

        if (r.gateT1 <= r.gateT0)
        {
            if (segSegDistXZ(a, b, r.a, r.b) < kFenceClear) return false;
            continue;
        }

        const int bays = (r.posts > 1) ? r.posts - 1 : 1;
        const int b0   = static_cast<int>(r.gateT0 * bays + 0.5f);
        const int b1   = static_cast<int>(r.gateT1 * bays + 0.5f);

        if (b0 >= 1 &&
            segSegDistXZ(a, b, r.a, fencePostAt(r, b0)) < kFenceClear)
            return false;

        if (b1 <= bays - 1 &&
            segSegDistXZ(a, b, fencePostAt(r, b1), r.b) < kFenceClear)
            return false;
    }
    return true;
}

/* Everything solid a walking leg has to miss. */
bool legClearOfSolids(const Vec3& a, const Vec3& b)
{
    return legClearOfStalls(a, b) && legClearOfFences(a, b);
}

/* And the same question for a single spot: a point is a leg of zero length. */
bool pointNearAnyFence(const Vec3& p)
{
    return !legClearOfFences(p, p);
}

/* --------------------------------------------------------------------------
 *  Nearest point outside every stall footprint.
 *
 *  Done in each stall's own local frame, where the box is axis aligned: the
 *  cheapest way out is over whichever of the four faces is nearest, so the
 *  body pops out of the side it was pushed in through rather than sliding the
 *  length of the counter.
 * ------------------------------------------------------------------------ */
Vec3 pushOutOfStalls(const Vec3& p)
{
    Vec3 out = p;

    /* Two passes: leaving one footprint can land inside a neighbour's. */
    for (int pass = 0; pass < 2; ++pass)
    {
        bool moved = false;

        for (int i = 0; i < kStallCount; ++i)
        {
            const StallSpec& s = kStallSpecs[i];

            float lx, lz;
            toStallLocal(s, out, lx, lz);

            const float hw = kStallHalfW + kWalkerHalf;
            const float hd = kStallHalfD + kWalkerHalf;

            if (std::fabs(lx) > hw || std::fabs(lz) > hd) continue;

            /* how far to each face, and which is closest */
            const float outX = hw - std::fabs(lx);
            const float outZ = hd - std::fabs(lz);

            if (outX < outZ) lx += (lx >= 0.0f) ? outX : -outX;
            else             lz += (lz >= 0.0f) ? outZ : -outZ;

            /* back to world: local +X is (cos a, -sin a), +Z is (sin a, cos a) */
            const float a  = s.rotY * 0.01745329f;
            const float c  = std::cos(a);
            const float sn = std::sin(a);

            out = Vec3(s.x + lx * c + lz * sn, 0.0f,
                       s.z - lx * sn + lz * c);
            moved = true;
        }

        if (!moved) break;
    }
    return out;
}

/* --------------------------------------------------------------------------
 *  A verge lane point that is guaranteed not to be inside a stall.
 *
 *  The outer walking lanes pass very close to stall 1, whose east corner sits
 *  only ~4.4 units from the dirt edge.  Rather than move the stall (its place
 *  in the composition is deliberate) the lane is pulled back toward the road
 *  until it is clear.  The base offset already reserves kVergeGap of grass, so
 *  even a fully collapsed stagger never puts anybody on the carriageway.
 * ------------------------------------------------------------------------ */
Vec3 vergeNodeSafe(int i, float side, float stagger)
{
    for (float shrink = 0.0f; shrink <= stagger + 0.001f; shrink += 0.30f)
    {
        const Vec3 p = vergeNode(i, side, stagger - shrink);
        if (!pointInAnyStall(p) && !pointNearAnyFence(p)) return p;
    }
    return vergeNode(i, side, 0.0f);
}

/* --------------------------------------------------------------------------
 *  Where a shopper waits before stepping up to a counter.
 *
 *  customerSpot() is already 1.8 units clear of the canopy; this pushes a
 *  little further out along the same normal.  Approaching down the stall's
 *  own facing direction means the last stride is always head-on to the
 *  counter, which is both collision free and better looking - the shopper
 *  ends up square to the stall instead of skidding in sideways.
 * ------------------------------------------------------------------------ */
Vec3 stallApproach(int stallIdx, float out)
{
    const StallSpec& s = kStallSpecs[stallIdx];
    const Vec3 f = stallFrontDir(s);
    const float d = kStallD * 0.5f + out;
    return Vec3(s.x + f.x * d, 0.0f, s.z + f.z * d);
}

/* --------------------------------------------------------------------------
 *  Append a waypoint, inserting a detour first if walking straight there
 *  would cut through a stall.
 *
 *  The verge lanes themselves are clear, but a leg that spans a bend - or the
 *  leg leaving a counter - can still clip a canopy.  Rather than give up, try
 *  progressively wider sidesteps perpendicular to the leg and take the first
 *  that gets there in two clear hops.  This is what stops a shopper walking
 *  out through the stall next door after browsing.
 *
 *  A sidestep is only accepted if it is also off the carriageway and out of
 *  the water.  It used to be tested against the stalls alone, which is how
 *  shoppers ended up with a waypoint at (-0.2, 7.3) - three quarters of a unit
 *  from the road centre line, i.e. standing in the middle of the traffic
 *  waiting to be run over.  A detour that dodges a canopy by stepping into the
 *  road has not solved anything.
 * ------------------------------------------------------------------------ */
bool pushLegStalls(std::vector<Vec3>& path, const Vec3& to)
{
    if (path.empty()) { path.push_back(to); return true; }

    const Vec3 from = path.back();
    if (legClearOfSolids(from, to)) { path.push_back(to); return true; }

    /* Perpendicular to the blocked leg, in the XZ plane. */
    const float dx  = to.x - from.x;
    const float dz  = to.z - from.z;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0e-4f) { path.push_back(to); return true; }

    const float px = dz / len;
    const float pz = -dx / len;

    /* Sidesteps are tried near-first and on both sides.  The range goes well
     * past the width of a stall because the detour also has to clear the cart
     * corridor and the river, and on the market side the gap between a canopy
     * and the road can be narrower than a stall is wide - a 14-unit reach used
     * to come up empty there and the shopper took the direct leg through the
     * canopy instead. */
    for (float step = 2.0f; step <= 26.0f; step += 1.5f)
    for (int    sgn = -1;   sgn <= 1;      sgn += 2)
    {
        const Vec3 mid(from.x + dx * 0.5f + px * step * sgn, 0.0f,
                       from.z + dz * 0.5f + pz * step * sgn);

        if (pointInAnyStall(mid))            continue;
        if (pointNearAnyFence(mid))          continue;
        if (inCartCorridor(mid, 0.0f))       continue;
        if (inOpenWater(mid, 0.0f))          continue;
        if (!legClearOfSolids(from, mid))    continue;
        if (!legClearOfSolids(mid, to))      continue;

        path.push_back(mid);
        path.push_back(to);
        return true;
    }

    /* No detour found.  Take the direct leg rather than drop the waypoint -
     * a shopper who cannot be routed is better off walking past than being
     * deleted - and report the failure so the caller can choose not to shop
     * here at all.  The audit harness asserts this never fires in this scene. */
    path.push_back(to);
    return false;
}

/* --------------------------------------------------------------------------
 *  The same, but it will not let anybody wade.
 *
 *  There is exactly one dry way across the channel - the bridge - so a leg
 *  that would cross open water is replaced by three: up the bank to the
 *  bridge approach on the walker's own side, over the deck, then on to where
 *  they were going.  Each of those is still routed round the stalls.
 * ------------------------------------------------------------------------ */
/* --------------------------------------------------------------------------
 *  Walk the footway across the deck.
 *
 *  The two ends of the bridge are not enough on their own: the road curves
 *  across the span, so the straight chord between the near and far footway
 *  drifts off the side of the deck and back into open water.  (It did exactly
 *  that - the east-verge exit leg tested clear at both ends and crossed the
 *  river in the middle.)  These follow the road's own samples, offset onto the
 *  walker's side, so the crossing tracks the deck instead of cutting the
 *  corner off it.
 * ------------------------------------------------------------------------ */
void appendDeckWalk(std::vector<Vec3>& path, float side, bool reversed)
{
    if (!gBridgeReady) return;

    const int lo = gBridgeI0;
    const int hi = gBridgeI1;

    for (int step = 0; step <= hi - lo; ++step)
    {
        const int i = reversed ? (hi - step) : (lo + step);

        const Vec3& c = gRoadPts[static_cast<std::size_t>(i)];

        /* road direction at i, so the offset lands on the right side */
        const int a = (i + 1 < static_cast<int>(gRoadPts.size())) ? i : i - 1;
        const int b = a + 1;
        const float dx = gRoadPts[static_cast<std::size_t>(b)].x -
                         gRoadPts[static_cast<std::size_t>(a)].x;
        const float dz = gRoadPts[static_cast<std::size_t>(b)].z -
                         gRoadPts[static_cast<std::size_t>(a)].z;
        const float l  = std::sqrt(dx * dx + dz * dz);
        if (l < 1.0e-4f) continue;

        /* left of travel is (dz, -dx) */
        const float nx = (dz / l) * side * kFootwayOff;
        const float nz = (-dx / l) * side * kFootwayOff;

        path.push_back(Vec3(c.x + nx, 0.0f, c.z + nz));
    }
}

bool pushLeg(std::vector<Vec3>& path, const Vec3& to, float side)
{
    if (path.empty()) { path.push_back(to); return true; }

    const Vec3 from = path.back();

    if (gBridgeReady && legInRiver(from, to, 0.0f))
    {
        /* Which end of the deck is on our side of the water? */
        const Vec3 e0 = bridgeFootway(side, 0);
        const Vec3 e1 = bridgeFootway(side, 1);
        const bool fromFar = (gh::distXZ(from, e1) < gh::distXZ(from, e0));

        bool ok = pushLegStalls(path, fromFar ? e1 : e0);
        appendDeckWalk(path, side, fromFar);       /* over the deck itself */
        ok = pushLegStalls(path, to) && ok;
        return ok;
    }

    return pushLegStalls(path, to);
}

/* "Can the shopper actually get there?" - the same question pushLeg answers,
 * asked without committing to a path.  Used while picking a branch node. */
bool legRoutable(const Vec3& a, const Vec3& b, float side)
{
    std::vector<Vec3> probe;
    probe.push_back(a);
    return pushLeg(probe, b, side);
}

/* --------------------------------------------------------------------------
 *  The market-side walking lane.
 *
 *  Past the bend the road runs BEHIND the stall row, and the strip between
 *  the dirt and stall 2 narrows to about 1.3 units - not a footpath, and any
 *  lane threaded through it either clipped the stall or put people in the
 *  road.  So on the market side the lane stops following the road there and
 *  follows the shop fronts instead, which is where a market crowd actually
 *  walks.  These points sit in front of every counter, clear of all six
 *  footprints, and the audit harness checks that every leg between them is
 *  clear before the scene is allowed to use them.
 * ------------------------------------------------------------------------ */
const int kFrontageFirst = 5;   /* first road node the frontage replaces */

/* The last point used to be (70, 8), which the river now runs straight
 * through - the market-side crowd walked into the water on their way off the
 * right edge of the frame.  The lane instead turns down the near bank and
 * leaves along it, so it stays dry, stays on camera and never has to cross. */
const Vec3 kFrontageLane[4] =
{
    Vec3(  4.5f, 0.0f,  8.5f),   /* west end of the row      */
    Vec3( 16.0f, 0.0f, 10.5f),   /* between stalls 2 and 3   */
    Vec3( 30.0f, 0.0f, 13.0f),   /* in front of the tea hut  */
    Vec3( 40.0f, 0.0f, 24.0f)    /* down the near bank, off frame */
};

/* Lane direction at frontage index k, used to spread the stagger sideways. */
Vec3 frontageNormal(int k)
{
    const int a = (k + 1 < 4) ? k : k - 1;
    const int b = a + 1;

    const float dx  = kFrontageLane[b].x - kFrontageLane[a].x;
    const float dz  = kFrontageLane[b].z - kFrontageLane[a].z;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0e-4f) return Vec3(0.0f, 0.0f, 1.0f);

    Vec3 n(dz / len, 0.0f, -dx / len);

    /* Take whichever normal leads AWAY from the road.
     *
     * This used to take whichever pointed toward the camera, on the reasoning
     * that a bigger stagger should push a walker further out from the shop
     * fronts.  At the west end of the row that is the wrong way round: the
     * road is on the +z side there, so a fully staggered walker was pushed
     * from (4.5, 8.5) to (3.9, 11.9) - 3.99 units from the centre line, well
     * inside the 3.95 a cart body sweeps.  That is the market-side crowd
     * walking into the traffic, and it is what put people under the horses.
     *
     * Measuring against the road instead is the same intent stated correctly:
     * more stagger is always further from whatever is driving past. */
    const Vec3  base  = kFrontageLane[k];
    const Vec3  plus (base.x + n.x * 2.0f, 0.0f, base.z + n.z * 2.0f);
    const Vec3  minus(base.x - n.x * 2.0f, 0.0f, base.z - n.z * 2.0f);

    float dPlus, dMinus, hw;
    roadNearest(plus,  dPlus,  hw);
    roadNearest(minus, dMinus, hw);

    if (dMinus > dPlus) { n.x = -n.x; n.z = -n.z; }
    return n;
}

/* --------------------------------------------------------------------------
 *  One pedestrian lane point.  side +1 keeps the road verge the whole way;
 *  side -1 is the market side and switches to the frontage past the bend.
 *
 *  Whatever the lane hands back is finally pushed clear of the carriageway:
 *  the verge margin is measured off the drawn dirt, and past the bend the
 *  dirt is narrower than the carts that use it.
 * ------------------------------------------------------------------------ */
Vec3 walkNode(int i, float side, float stagger)
{
    if (side > 0.0f || i < kFrontageFirst)
        return keepOffRoad(vergeNodeSafe(i, side, stagger));

    const int k = i - kFrontageFirst;
    const Vec3 base = kFrontageLane[k];
    const Vec3 n    = frontageNormal(k);

    for (float shrink = 0.0f; shrink <= stagger + 0.001f; shrink += 0.30f)
    {
        const float off = stagger - shrink;
        const Vec3  p(base.x + n.x * off, 0.0f, base.z + n.z * off);
        if (!pointInAnyStall(p)) return keepOffRoad(p);
    }
    return keepOffRoad(base);
}

/* --------------------------------------------------------------------------
 *  One waypoint past the end of the walk, far enough out that a villager
 *  reaching it - and so vanishing - is outside the frame whatever shape the
 *  window is.
 *
 *  gluPerspective pins the VERTICAL field of view, so screen y is the axis a
 *  wider window cannot move.  The market-side lane already runs down the near
 *  bank toward the camera, so carrying it further takes it off the BOTTOM edge
 *  (screen y -1.2), which no aspect ratio undoes.  The far verge runs along the
 *  horizon where only screen x is available, so it is carried as far right as
 *  the ground goes: off frame at 16:9 and narrower.
 *
 *  The direction comes from the lane geometry rather than from the path's last
 *  leg, because pushLeg() may have made that leg a sidestep around a canopy,
 *  which points nowhere useful.  The result is clamped inside kGround* so
 *  nobody walks off the edge of the world getting there.
 * ------------------------------------------------------------------------ */
Vec3 walkOffFrame(float side, const Vec3& last)
{
    float dx, dz;

    if (side < 0.0f)
    {
        dx = kFrontageLane[3].x - kFrontageLane[2].x;
        dz = kFrontageLane[3].z - kFrontageLane[2].z;
    }
    else
    {
        dx = kRoad[kRoadNodes - 1].x - kRoad[kRoadNodes - 2].x;
        dz = kRoad[kRoadNodes - 1].z - kRoad[kRoadNodes - 2].z;
    }

    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0e-4f) return last;

    Vec3 out(last.x + (dx / len) * 26.0f, 0.0f,
             last.z + (dz / len) * 26.0f);

    if (out.x < kGroundMinX + 1.0f) out.x = kGroundMinX + 1.0f;
    if (out.x > kGroundMaxX - 1.0f) out.x = kGroundMaxX - 1.0f;
    if (out.z < kGroundMinZ + 1.0f) out.z = kGroundMinZ + 1.0f;
    if (out.z > kGroundMaxZ - 1.0f) out.z = kGroundMaxZ - 1.0f;

    return out;
}

/* --------------------------------------------------------------------------
 *  Kids Playing Football State
 * ------------------------------------------------------------------------ */
/* ==========================================================================
 *  The tethered goat - bottom-right corner of the frame
 *  --------------------------------------------------------------------------
 *  A voxel goat cropping the grass on the green between the market and the
 *  foreground fence.  The spot is chosen to be ~25 units clear of the road,
 *  well away from the stalls, the cottages, the trees and the children's
 *  pitch, and to sit in the lower-right corner of the fixed camera's view.
 *
 *  EP3 : it alternates between GRAZING (head down, small chewing bob) and
 *        STEPPING (a short walk to a fresh patch), on randomised timers.
 *  EP4 : head hangs off the neck which hangs off the body, so lowering the
 *        neck to graze carries the head, muzzle, ears and horns with it.
 * ==========================================================================*/
const float kGoatHomeX = 30.0f;
const float kGoatHomeZ = 21.0f;
const float kGoatRoam  =  3.4f;   /* never wanders further than this      */

enum GoatState { GOAT_GRAZE, GOAT_STEP };

GoatState gGoatState  = GOAT_GRAZE;
float gGoatX          = kGoatHomeX;
float gGoatZ          = kGoatHomeZ;
float gGoatHeading    = 205.0f;
float gGoatTimer      = 3.0f;
float gGoatTgtX       = kGoatHomeX;
float gGoatTgtZ       = kGoatHomeZ;
float gGoatChew       = 0.0f;
float gGoatNeck       = 1.0f;     /* 0 = head up, 1 = fully down grazing  */
float gGoatWalk       = 0.0f;
float gGoatTail       = 0.0f;

void updateGoat(float dt, float night)
{
    gGoatChew += dt * 5.5f;
    gGoatTail += dt * 2.2f;
    gGoatTimer -= dt;

    /* At night the goat settles: it stops wandering and keeps its head down. */
    const bool resting = (night > 0.55f);

    switch (gGoatState)
    {
        case GOAT_GRAZE:
            /* ease the neck down to the grass */
            gGoatNeck += (1.0f - gGoatNeck) * std::min(1.0f, dt * 2.5f);

            if (gGoatTimer <= 0.0f && !resting)
            {
                /* pick a fresh patch inside the tether radius */
                const float a = gh::randRange(0.0f, 6.2831853f);
                const float r = gh::randRange(1.2f, kGoatRoam);
                gGoatTgtX = kGoatHomeX + std::cos(a) * r;
                gGoatTgtZ = kGoatHomeZ + std::sin(a) * r * 0.7f;
                gGoatState = GOAT_STEP;
                gGoatTimer = gh::randRange(2.5f, 5.0f);
            }
            break;

        case GOAT_STEP:
        {
            /* head comes up while walking */
            gGoatNeck += (0.15f - gGoatNeck) * std::min(1.0f, dt * 4.0f);

            const float dx = gGoatTgtX - gGoatX;
            const float dz = gGoatTgtZ - gGoatZ;
            const float d  = std::sqrt(dx * dx + dz * dz);

            if (d < 0.18f || gGoatTimer <= 0.0f)
            {
                gGoatState = GOAT_GRAZE;
                gGoatTimer = gh::randRange(4.0f, 9.0f);
                gGoatWalk  = 0.0f;
            }
            else
            {
                const float sp = 1.15f;
                gGoatX += (dx / d) * sp * dt;
                gGoatZ += (dz / d) * sp * dt;
                gGoatWalk += dt * 6.0f;

                const float want = gh::headingXZ(Vec3(gGoatX, 0.0f, gGoatZ),
                                                 Vec3(gGoatTgtX, 0.0f, gGoatTgtZ));
                float turn = want - gGoatHeading;
                while (turn >  180.0f) turn -= 360.0f;
                while (turn < -180.0f) turn += 360.0f;
                gGoatHeading += turn * std::min(1.0f, dt * 3.5f);
            }
            break;
        }
    }
}

void drawGoat()
{
    const Color coat (0.90f, 0.89f, 0.85f);   /* off-white               */
    const Color dark (0.42f, 0.38f, 0.34f);   /* legs, muzzle, ears      */
    const Color horn (0.62f, 0.56f, 0.44f);

    const float bodyY = 1.30f;
    const float legLen = 1.05f;

    /* Grazing dips the neck; the chew is a small bob on top of it. */
    const float neckPitch = 22.0f + gGoatNeck * 46.0f
                          + (gGoatState == GOAT_GRAZE
                                 ? std::sin(gGoatChew) * 2.5f : 0.0f);
    const float legSwing = (gGoatState == GOAT_STEP)
                         ? 18.0f * std::sin(gGoatWalk) : 0.0f;

    glPushMatrix();
        glTranslatef(gGoatX, 0.0f, gGoatZ);
        glRotatef(gGoatHeading, 0.0f, 1.0f, 0.0f);

        /* ---- barrel ---------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, bodyY, 0.0f);
            gh::drawBlock(0.86f, 0.92f, 2.05f, coat);
        glPopMatrix();

        /* ---- neck -> head (EP4: the head rides on the neck) ------------ */
        glPushMatrix();
            glTranslatef(0.0f, bodyY + 0.30f, 0.92f);
            glRotatef(neckPitch, 1.0f, 0.0f, 0.0f);

            glPushMatrix();
                glTranslatef(0.0f, 0.42f, 0.0f);
                gh::drawBlock(0.46f, 0.95f, 0.50f, coat);
            glPopMatrix();

            /* head frame at the top of the neck */
            glPushMatrix();
                glTranslatef(0.0f, 0.92f, 0.10f);
                glRotatef(38.0f, 1.0f, 0.0f, 0.0f);

                gh::drawBlock(0.48f, 0.46f, 0.72f, coat);

                glPushMatrix();                       /* muzzle */
                    glTranslatef(0.0f, -0.06f, 0.46f);
                    gh::drawBlock(0.34f, 0.30f, 0.30f, dark);
                glPopMatrix();

                for (int e = -1; e <= 1; e += 2)      /* ears */
                {
                    glPushMatrix();
                        glTranslatef(e * 0.32f, 0.10f, -0.06f);
                        glRotatef(e * 26.0f, 0.0f, 0.0f, 1.0f);
                        gh::drawBlock(0.34f, 0.14f, 0.24f, dark);
                    glPopMatrix();
                }
                for (int h = -1; h <= 1; h += 2)      /* horns */
                {
                    glPushMatrix();
                        glTranslatef(h * 0.14f, 0.34f, -0.16f);
                        glRotatef(-24.0f, 1.0f, 0.0f, 0.0f);
                        gh::drawBlock(0.12f, 0.42f, 0.12f, horn);
                    glPopMatrix();
                }
            glPopMatrix();
        glPopMatrix();

        /* ---- beard ----------------------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, bodyY + 0.10f, 1.20f);
            gh::drawBlock(0.16f, 0.34f, 0.14f, coat);
        glPopMatrix();

        /* ---- tail ------------------------------------------------------ */
        glPushMatrix();
            glTranslatef(0.0f, bodyY + 0.42f, -1.02f);
            glRotatef(-30.0f + std::sin(gGoatTail) * 14.0f, 1.0f, 0.0f, 0.0f);
            glTranslatef(0.0f, 0.16f, 0.0f);
            gh::drawBlock(0.18f, 0.36f, 0.16f, coat);
        glPopMatrix();

        /* ---- four legs, diagonal pairs --------------------------------- */
        for (int f = 0; f < 2; ++f)
        for (int side = -1; side <= 1; side += 2)
        {
            const bool inPhase = ((f == 1) == (side < 0));
            const float a = inPhase ? legSwing : -legSwing;

            glPushMatrix();
                glTranslatef(side * 0.30f, legLen, (f == 1) ? 0.66f : -0.66f);
                glRotatef(a, 1.0f, 0.0f, 0.0f);
                glTranslatef(0.0f, -legLen * 0.5f, 0.0f);
                gh::drawBlock(0.24f, legLen, 0.24f, dark);
            glPopMatrix();
        }

    glPopMatrix();
}

/* ==========================================================================
 *  The old boats on the river
 *  --------------------------------------------------------------------------
 *  Two weathered clinker-built rowing boats drifting slowly downstream, each
 *  rocking and bobbing on its own phase so they never move as a pair.  They
 *  are placed by a fraction of the way along the river spline rather than by
 *  world coordinates, so they follow the channel automatically if it is ever
 *  re-routed, and they wrap around at the end rather than piling up.
 *
 *  Animated, so they are drawn from drawScene() and NOT from the static
 *  display list, which would freeze them mid-stream.
 * ==========================================================================*/
const float kBoatDrift    = 0.0060f;   /* fraction of the river per second  */
const float kBoatRockAmp  = 4.5f;      /* roll amplitude, degrees           */
const float kBoatBobAmp   = 0.07f;     /* vertical bob, world units         */


const int kBoatCount = 2;

bool  gBoatsReady   = false;
float gBoatT[kBoatCount]     = { 0.60f, 0.86f };  /* along the river      */
float gBoatPhase[kBoatCount] = { 0.0f, 2.7f };

void initBoats()
{
    gBoatT[0] = 0.08f;  gBoatPhase[0] = 0.0f;
    gBoatT[1] = 0.24f;  gBoatPhase[1] = 2.7f;
    gBoatsReady = true;
}

void updateBoats(float dt)
{
    if (!gBoatsReady) initBoats();

    for (int i = 0; i < kBoatCount; ++i)
    {
        gBoatPhase[i] += dt * (1.5f + 0.3f * static_cast<float>(i));
        gBoatT[i]     += kBoatDrift * dt;

        /* Back to the top of the reach once past the frame.
         *
         * This used to wrap to t = 0.50, which is INSIDE the stretch the
         * bridge covers - so a boat was immediately bumped to the far side of
         * the deck and spent its whole life shuttling between the crossing and
         * the off-camera tail.  Both boats were off screen most of the time.
         * Wrapping to the top instead sends them down the visible reach, and
         * wrapping at 0.70 rather than 0.99 skips the tail, which is already
         * off the right edge of the frame. */
        if (gBoatT[i] > 0.70f) gBoatT[i] = 0.02f;

        /* Step over the bridge rather than through its piers. */
        if (gBoatT[i] > gBoatSkipLo && gBoatT[i] < gBoatSkipHi)
            gBoatT[i] = gBoatSkipHi;
    }
}

/* Sample the river spline at t in [0,1]: position and unit direction. */
void riverAt(float t, Vec3& pos, Vec3& dir)
{
    const int n = static_cast<int>(gRiverPts.size());
    if (n < 2) { pos = Vec3(); dir = Vec3(0.0f, 0.0f, 1.0f); return; }

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const float s = t * static_cast<float>(n - 1);
    int i0 = static_cast<int>(s);
    if (i0 > n - 2) i0 = n - 2;
    const int   i1 = i0 + 1;
    const float f  = s - static_cast<float>(i0);

    const Vec3& a = gRiverPts[static_cast<std::size_t>(i0)];
    const Vec3& b = gRiverPts[static_cast<std::size_t>(i1)];

    pos = Vec3(gh::lerp(a.x, b.x, f), 0.0f, gh::lerp(a.z, b.z, f));

    const float ex = b.x - a.x, ez = b.z - a.z;
    const float el = std::sqrt(ex * ex + ez * ez);
    dir = (el > 1.0e-4f) ? Vec3(ex / el, 0.0f, ez / el)
                         : Vec3(0.0f, 0.0f, 1.0f);
}

/* --------------------------------------------------------------------------
 *  One boat, drawn about its own waterline.
 *
 *  Local +Z runs from stern to bow, +X is athwartships, and y = 0 is the
 *  water: the hull is drawn straddling it so the boat sits IN the river
 *  rather than on top of it, and only the sheer strake, the thwarts and the
 *  oars stand above the surface.
 * ------------------------------------------------------------------------ */
void drawBoatBody(int variant)
{
    const float halfLen = 3.4f;
    const float halfWid = 1.00f;
    const float draught = 0.14f;    /* hull below the waterline            */
    const float sheer   = 0.52f;    /* freeboard above it                  */

    /* Old timber: the second boat is greyer, as if it has sat out longer. */
    const Color hull  = (variant == 0) ? kBoatHull
                                       : gh::mixColor(kBoatHull,
                                                      Color(0.44f, 0.42f, 0.38f),
                                                      0.45f);
    const Color trim  = (variant == 0) ? kBoatTrim
                                       : gh::mixColor(kBoatTrim,
                                                      Color(0.38f, 0.36f, 0.33f),
                                                      0.45f);

    /* main hull */
    glPushMatrix();
        glTranslatef(0.0f, (sheer - draught) * 0.5f, 0.0f);
        gh::drawBlock(halfWid * 2.0f, draught + sheer, halfLen * 2.0f, hull);
    glPopMatrix();

    /* the open interior, a shade darker and inset so the boat reads hollow */
    glPushMatrix();
        glTranslatef(0.0f, sheer * 0.45f, 0.0f);
        gh::drawBlock(halfWid * 2.0f - 0.42f, 0.30f, halfLen * 2.0f - 0.55f,
                      gh::shade(kBoatInner, 0.78f));
    glPopMatrix();

    /* sheer strake - the lighter rim running right round the gunwale */
    glPushMatrix();
        glTranslatef(0.0f, sheer * 0.72f, 0.0f);
        gh::drawBlock(halfWid * 2.0f + 0.16f, 0.20f, halfLen * 2.0f + 0.12f,
                      trim);
    glPopMatrix();

    /* raised bow and stern posts, narrower so the ends read as tapered */
    for (int sgn = -1; sgn <= 1; sgn += 2)
    {
        glPushMatrix();
            glTranslatef(0.0f, sheer * 0.95f, sgn * halfLen * 0.86f);
            gh::drawBlock(halfWid * 2.0f - 0.55f, 0.42f, 0.66f, hull);
        glPopMatrix();
    }

    /* three thwarts */
    for (int i = 0; i < 3; ++i)
    {
        glPushMatrix();
            glTranslatef(0.0f, sheer * 0.66f, -2.0f + 2.0f * static_cast<float>(i));
            gh::drawBlock(halfWid * 2.0f - 0.18f, 0.15f, 0.34f,
                          gh::shade(trim, 1.14f));
        glPopMatrix();
    }

    /* a pair of oars shipped over the side, blades flat on the water */
    for (int sgn = -1; sgn <= 1; sgn += 2)
    {
        glPushMatrix();
            glTranslatef(sgn * halfWid * 0.75f, sheer * 0.80f, -0.6f);
            glRotatef(sgn * 18.0f, 0.0f, 1.0f, 0.0f);

            glPushMatrix();                       /* shaft */
                glTranslatef(sgn * 1.05f, 0.0f, 0.0f);
                gh::drawBlock(2.1f, 0.11f, 0.11f, gh::shade(trim, 0.88f));
            glPopMatrix();

            glPushMatrix();                       /* blade */
                glTranslatef(sgn * 2.25f, -0.02f, 0.0f);
                gh::drawBlock(0.62f, 0.07f, 0.40f, gh::shade(trim, 1.05f));
            glPopMatrix();
        glPopMatrix();
    }
}

void drawBoats()
{
    if (!gBoatsReady || gRiverPts.size() < 2) return;

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    for (int i = 0; i < kBoatCount; ++i)
    {
        Vec3 p, dir;
        riverAt(gBoatT[i], p, dir);

        const float yaw = std::atan2(dir.x, dir.z) * 180.0f /
                          static_cast<float>(M_PI);
        const float roll = std::sin(gBoatPhase[i]) * kBoatRockAmp;
        const float bob  = std::sin(gBoatPhase[i] * 0.77f + 1.1f) * kBoatBobAmp;

        glPushMatrix();
            /* Sit the hull on the water SURFACE, not on kWaterTopY: the
             * channel is a stacked slab like everything else in this scene, so
             * anything much below the surface disappears into the terrain
             * underneath it rather than reading as submerged. */
            glTranslatef(p.x, kWaterTopY + 0.05f + bob, p.z);
            glRotatef(yaw,  0.0f, 1.0f, 0.0f);
            glRotatef(roll, 0.0f, 0.0f, 1.0f);
            drawBoatBody(i);
        glPopMatrix();
    }

    gh::setOutlineEnabled(prevOutline);
}

/* --------------------------------------------------------------------------
 *  The children's football game, and where the children go at night.
 *
 *  The ball used to bounce between a fixed pair with two others watching.  Now
 *  five children stand in a ring and the ball is passed to a randomly chosen
 *  DIFFERENT child each time, so over a few seconds it reaches all of them,
 *  and every child turns to follow whoever has it.
 *
 *  The group is a state machine (EP3).  At dusk the game breaks up and they
 *  walk home into the cottage; at dawn they come back out and resume:
 *
 *      PLAYING --(night)--> GOING_HOME --> INSIDE --(dawn)--> RETURNING
 *         ^                                                      |
 *         +------------------------------------------------------+
 * ------------------------------------------------------------------------ */
enum PlayState { PLAY_PLAYING, PLAY_GOING_HOME, PLAY_INSIDE, PLAY_RETURNING };

const int kKidCount = 5;

/* --------------------------------------------------------------------------
 *  Ring of play positions, on the open green WEST of the road - screen left.
 *
 *  The pitch used to sit at (12, 26.5), which is east of the road: the
 *  children played on the wrong side of the carriageway from the cottage they
 *  live in, so twice a day the whole group crossed the traffic to get home and
 *  back.  It is now on the same side as the houses, which is both where
 *  children in a village actually play and what removes the crossing entirely.
 *
 *  These numbers are not hand-placed.  A search over centre, size and rotation
 *  picked the roomiest ring on that side that satisfies all of at once: every
 *  child inside the fixed camera's frame, 1.4 units clear of every stall
 *  canopy, outside the corridor a cart body sweeps, 1.7 clear of the west
 *  walking lane, and with an unobstructed sightline from the camera - a pitch
 *  hidden behind the market row is a pitch nobody sees.  The ring is an
 *  ellipse turned 68.8 degrees because the clear ground there is a diagonal
 *  strip between the road and the cottages; the tightest gap between any two
 *  children comes out at 3.06 units.
 * ------------------------------------------------------------------------ */
const float kPlayCX  = -26.0f;
const float kPlayCZ  =  12.5f;
const float kPlayRX  =   4.0f;   /* ring semi-axes before rotation         */
const float kPlayRZ  =   2.6f;
const float kPlayRot =   1.20f;  /* radians, about +Y                      */

/* --------------------------------------------------------------------------
 *  The route home.
 *
 *  The pitch and the cottage are now on the SAME side of the road, so the walk
 *  home no longer crosses it - it runs west along the green, behind the market
 *  row, straight to the door.  The old route went (-12,8) -> (-30,8) -> door,
 *  which started east of the carriageway and cut over it on the first leg.
 *
 *  kDoor matches drawHouse(): the door sits at local
 *  x = -wallW/2 + 1.85 on the +Z face, scaled by 1.55 and rotated -8 degrees.
 * ------------------------------------------------------------------------ */
const float kHome1X = -30.0f;   /* west off the pitch, clear of stall 6  */
const float kHome1Z =   6.0f;
const float kHome2X = -35.0f;   /* turn in toward the cottage front      */
const float kHome2Z =  -2.0f;
const float kDoorX  = -36.8f;   /* the cottage doorstep                  */
const float kDoorZ  =  -9.5f;

struct Kid
{
    float   px, pz;        /* the spot this child plays on            */
    float   x,  z;         /* where the child is right now            */
    float   heading;       /* degrees about +Y                        */
    float   walkPhase;     /* leg cycle while walking                 */
    float   kickSwing;     /* decays after a kick                     */
    float   cheer;         /* small hop when the ball comes their way  */
    int     leg;           /* which leg of the route home they are on */
    CustomerLook look;
};

Kid   gKids[kKidCount];
bool  gKidsReady = false;

PlayState gPlayState = PLAY_PLAYING;

/* Ball state: it is always either in flight between two children, or resting
 * at the feet of whoever last received it. */
int   gHolder   = 0;
int   gReceiver = 1;
bool  gInFlight = false;
float gFlightT  = 0.0f;      /* 0..1 across the current pass          */
float gFlightLen = 1.0f;
float gDwell    = 0.6f;      /* pause before passing it on            */
float gBallX = 0.0f, gBallY = 0.0f, gBallZ = 0.0f;
float gBallSpin = 0.0f;

/* Heading of the current pass, degrees about +Y.  The ball rolls about the
 * axis across this, so the spin tumbles end-over-end along the flight line
 * rather than barrel-rolling around it. */
float gBallHeading = 0.0f;
float gKidsTime = 0.0f;

/* Lay the children out in a ring and give each one a look. */
void initKids()
{
    static const float kShirts[kKidCount][3] =
    {
        {0.95f, 0.20f, 0.20f}, {0.15f, 0.85f, 0.20f}, {0.95f, 0.80f, 0.15f},
        {0.15f, 0.60f, 0.90f}, {0.85f, 0.40f, 0.85f}
    };
    static const float kPants[kKidCount][3] =
    {
        {0.14f, 0.20f, 0.82f}, {0.20f, 0.20f, 0.20f}, {0.30f, 0.30f, 0.30f},
        {0.40f, 0.20f, 0.10f}, {0.18f, 0.28f, 0.22f}
    };

    const float ct = std::cos(kPlayRot);
    const float st = std::sin(kPlayRot);

    for (int i = 0; i < kKidCount; ++i)
    {
        const float a = (static_cast<float>(i) / kKidCount) * 6.2831853f;
        Kid& k = gKids[i];

        /* point on the ellipse, then turned into the diagonal strip of clear
         * ground the pitch actually occupies */
        const float ux = std::cos(a) * kPlayRX;
        const float uz = std::sin(a) * kPlayRZ;

        k.px = kPlayCX + ux * ct - uz * st;
        k.pz = kPlayCZ + ux * st + uz * ct;
        k.x  = k.px;
        k.z  = k.pz;
        k.heading   = 0.0f;
        k.walkPhase = gh::randRange(0.0f, 6.28f);
        k.kickSwing = 0.0f;
        k.cheer     = 0.0f;
        k.leg       = 0;

        k.look.shirt    = Color(kShirts[i][0], kShirts[i][1], kShirts[i][2]);
        k.look.trousers = Color(kPants[i][0],  kPants[i][1],  kPants[i][2]);
        k.look.scale    = 0.58f + 0.03f * static_cast<float>(i % 3);
    }

    gBallX = gKids[0].px;
    gBallZ = gKids[0].pz;
    gKidsReady = true;
}

/* Pick the next receiver: anybody except whoever is holding the ball, so it
 * genuinely circulates instead of ping-ponging between a fixed pair. */
int pickReceiver(int holder)
{
    const int step = gh::randInt(1, kKidCount - 1);
    return (holder + step) % kKidCount;
}

/* Turn a child smoothly toward a point. */
void faceToward(Kid& k, float tx, float tz, float dt)
{
    const float want = gh::headingXZ(Vec3(k.x, 0.0f, k.z), Vec3(tx, 0.0f, tz));
    float d = want - k.heading;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    k.heading += d * std::min(1.0f, dt * 6.0f);
}

/* Walk a child toward a target; returns true once it has arrived. */
bool walkToward(Kid& k, float tx, float tz, float speed, float dt)
{
    const float dx = tx - k.x;
    const float dz = tz - k.z;
    const float d  = std::sqrt(dx * dx + dz * dz);

    if (d < 0.35f) return true;

    k.x += (dx / d) * speed * dt;
    k.z += (dz / d) * speed * dt;
    k.walkPhase += dt * speed * 2.6f;
    faceToward(k, tx, tz, dt);
    return false;
}

/* --------------------------------------------------------------------------
 *  night  : 0 = broad daylight, 1 = fully dark (gh::nightFactor()).
 *  The children head home once it is properly dusk and come back at dawn.
 * ------------------------------------------------------------------------ */
void updateKidsAndFootball(float dt, float night)
{
    if (!gKidsReady) initKids();

    gKidsTime += dt * 4.0f;

    /* ---- group state machine ------------------------------------------ */
    switch (gPlayState)
    {
        case PLAY_PLAYING:
            if (night > 0.55f)
            {
                gPlayState = PLAY_GOING_HOME;
                for (int i = 0; i < kKidCount; ++i) gKids[i].leg = 0;
            }
            break;

        case PLAY_GOING_HOME:
        {
            bool allIn = true;
            for (int i = 0; i < kKidCount; ++i)
            {
                Kid& k = gKids[i];
                /* Stagger the speeds a little so they do not move as a single
                 * block: the group strings out along the route. */
                const float speed = 3.2f + 0.18f * static_cast<float>(i);

                switch (k.leg)
                {
                    case 0:
                        if (walkToward(k, kHome1X, kHome1Z, speed, dt)) k.leg = 1;
                        allIn = false;
                        break;
                    case 1:
                        if (walkToward(k, kHome2X, kHome2Z, speed, dt)) k.leg = 2;
                        allIn = false;
                        break;
                    case 2:
                        if (walkToward(k, kDoorX, kDoorZ, speed, dt)) k.leg = 3;
                        else allIn = false;
                        break;
                    default:
                        break;               /* leg 3: inside */
                }
            }
            if (allIn) gPlayState = PLAY_INSIDE;
            break;
        }

        case PLAY_INSIDE:
            if (night < 0.20f)
            {
                gPlayState = PLAY_RETURNING;
                for (int i = 0; i < kKidCount; ++i) gKids[i].leg = 0;
            }
            break;

        case PLAY_RETURNING:
        {
            bool allOut = true;
            for (int i = 0; i < kKidCount; ++i)
            {
                Kid& k = gKids[i];
                const float speed = 3.2f + 0.18f * static_cast<float>(i);

                /* the same three legs, walked back the other way */
                switch (k.leg)
                {
                    case 0:
                        if (walkToward(k, kHome2X, kHome2Z, speed, dt)) k.leg = 1;
                        allOut = false;
                        break;
                    case 1:
                        if (walkToward(k, kHome1X, kHome1Z, speed, dt)) k.leg = 2;
                        allOut = false;
                        break;
                    case 2:
                        if (walkToward(k, k.px, k.pz, speed, dt)) k.leg = 3;
                        else allOut = false;
                        break;
                    default:
                        break;
                }
            }
            if (allOut)
            {
                gPlayState = PLAY_PLAYING;
                gInFlight  = false;
                gDwell     = 0.5f;
                gBallX     = gKids[gHolder].x;
                gBallZ     = gKids[gHolder].z;
            }
            break;
        }
    }

    /* Limbs relax whatever the state. */
    for (int i = 0; i < kKidCount; ++i)
    {
        Kid& k = gKids[i];
        k.kickSwing += (0.0f - k.kickSwing) * std::min(1.0f, dt * 6.0f);
        k.cheer     += (0.0f - k.cheer)     * std::min(1.0f, dt * 3.0f);
    }

    /* The ball only lives while the game is on. */
    if (gPlayState != PLAY_PLAYING) return;

    /* ---- the pass ------------------------------------------------------ */
    if (gInFlight)
    {
        const Kid& from = gKids[gHolder];
        const Kid& to   = gKids[gReceiver];

        /* Flight time scales with distance so a long pass is not instant. */
        gFlightT += dt * (7.4f / std::max(2.0f, gFlightLen));

        if (gFlightT >= 1.0f)
        {
            gFlightT  = 1.0f;
            gInFlight = false;
            gDwell    = gh::randRange(0.45f, 1.10f);

            gHolder = gReceiver;              /* possession changes hands */
            gKids[gHolder].cheer = 1.0f;      /* small hop on receiving   */
        }

        const float t = gFlightT;
        gBallX = gh::lerp(from.x, to.x, t);
        gBallZ = gh::lerp(from.z, to.z, t);
        gBallY = std::sin(t * 3.14159265f) * (0.9f + gFlightLen * 0.055f);
        gBallSpin += dt * 520.0f;

        /* Everybody tracks the ball - that is what makes five children read
         * as one game rather than five idle figures. */
        for (int i = 0; i < kKidCount; ++i)
            faceToward(gKids[i], gBallX, gBallZ, dt);
    }
    else
    {
        /* Ball resting at the holder's feet. */
        const Kid& h = gKids[gHolder];
        gBallX = h.x + std::sin(h.heading * 0.01745329f) * 0.95f;
        gBallZ = h.z + std::cos(h.heading * 0.01745329f) * 0.95f;
        gBallY = 0.0f;

        gDwell -= dt;
        if (gDwell <= 0.0f)
        {
            gReceiver = pickReceiver(gHolder);

            const float dx = gKids[gReceiver].x - h.x;
            const float dz = gKids[gReceiver].z - h.z;
            gFlightLen = std::sqrt(dx * dx + dz * dz);

            /* the pass direction, so the ball rolls the way it travels */
            gBallHeading = gh::headingXZ(Vec3(h.x, 0.0f, h.z),
                                         Vec3(gKids[gReceiver].x, 0.0f,
                                              gKids[gReceiver].z));

            gInFlight = true;
            gFlightT  = 0.0f;
            gKids[gHolder].kickSwing = 62.0f;      /* the kick */
            faceToward(gKids[gHolder], gKids[gReceiver].x,
                       gKids[gReceiver].z, 1.0f);
        }

        for (int i = 0; i < kKidCount; ++i)
            faceToward(gKids[i], gBallX, gBallZ, dt);
    }
}

/* --------------------------------------------------------------------------
 *  The children, as far as the rest of the crowd is concerned.
 *
 *  They used to be invisible to everything outside this block: the separation
 *  pass never saw them, so an adult walked straight through a child, and the
 *  cart drivers never saw them either.  These few accessors are what let
 *  updateScene() fold them into the same rules everybody else obeys.
 * ------------------------------------------------------------------------ */
int  kidCount()     { return kKidCount; }
bool kidsOutdoors() { return gKidsReady && gPlayState != PLAY_INSIDE; }

Vec3 kidPos(int i)  { return Vec3(gKids[i].x, 0.0f, gKids[i].z); }

void kidNudge(int i, float dx, float dz)
{
    gKids[i].x += dx;
    gKids[i].z += dz;
}

void drawKidsAndFootball()
{
    if (!gKidsReady) return;

    /* Indoors: nothing to draw but the lit window, which the house owns. */
    if (gPlayState == PLAY_INSIDE) return;

    const bool playing = (gPlayState == PLAY_PLAYING);

    for (int i = 0; i < kKidCount; ++i)
    {
        const Kid& k = gKids[i];

        /* Walking home swings the legs; standing in the ring does not. */
        const bool walking = !playing;
        const float legSwing = walking
            ? 30.0f * std::sin(k.walkPhase)
            : k.kickSwing;
        const float armSwing = walking
            ? -22.0f * std::sin(k.walkPhase)
            : (-18.0f * k.cheer);

        const float hop = playing ? std::fabs(std::sin(gKidsTime)) * 0.55f * k.cheer
                                  : 0.0f;

        glPushMatrix();
            glTranslatef(k.x, hop, k.z);
            glRotatef(k.heading, 0.0f, 1.0f, 0.0f);
            glScalef(k.look.scale, k.look.scale, k.look.scale);
            drawHuman(k.look, armSwing, legSwing, 0.0f);
        glPopMatrix();
    }

    if (!playing) return;              /* the ball goes in with them */

    /* ----------------------------------------------------------------------
     *  The football.
     *
     *  It used to be two overlapping cuboids - a white box with a black box
     *  through it - which read as a crate being kicked about rather than as a
     *  ball.  It is now built with the project's own voxel-sphere rasteriser
     *  (EP1), the same primitive the tree canopies and the sun core use, so it
     *  is genuinely round within the voxel look of the scene rather than
     *  round-ish.  radius 3 at 0.16 units per voxel gives a ball a little under
     *  a unit across - right for a child's football beside a 4.4-unit adult.
     *
     *  The dark panels are a second, slightly smaller shell drawn at the six
     *  poles, which is what makes it read as a football and not a snowball,
     *  and it is the panels moving that sells the spin.
     * -------------------------------------------------------------------- */
    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);      /* no wireframe on a ball this small */

    const float vox = 0.16f;

    glPushMatrix();
        glTranslatef(gBallX, gBallY + 0.48f, gBallZ);

        /* Roll about the axis ACROSS the direction of travel.
         *
         * A ball spinning about a fixed world axis barrel-rolls along its own
         * flight line instead of rolling: turn to face the way it is going
         * first, then roll about that frame's X, and the panels tumble
         * end-over-end the way a kicked ball does.  gBallHeading is the
         * direction of the current pass, so the roll axis follows it. */
        glRotatef(gBallHeading, 0.0f, 1.0f, 0.0f);
        glRotatef(gBallSpin,    1.0f, 0.0f, 0.0f);

        gh::drawVoxelSphere(0.0f, 0.0f, 0.0f, 3, vox,
                            Color(0.96f, 0.96f, 0.94f));

        /* six dark panels, one at each pole of the shell */
        const float p = vox * 3.0f;
        const float pk[6][3] =
        {
            {  0.0f,  p,  0.0f }, {  0.0f, -p,  0.0f },
            {  p,  0.0f,  0.0f }, { -p,  0.0f,  0.0f },
            {  0.0f,  0.0f,  p }, {  0.0f,  0.0f, -p }
        };
        for (int i = 0; i < 6; ++i)
        {
            glPushMatrix();
                glTranslatef(pk[i][0] * 0.92f, pk[i][1] * 0.92f,
                             pk[i][2] * 0.92f);
                gh::drawBlock(vox * 1.7f, vox * 1.7f, vox * 1.7f,
                              Color(0.12f, 0.12f, 0.14f));
            glPopMatrix();
        }
    glPopMatrix();

    gh::setOutlineEnabled(prevOutline);
}


/* ==========================================================================
 *  Crowd separation - one solver for everybody on foot
 *  --------------------------------------------------------------------------
 *  Villagers, shoppers and the five children could walk clean through one
 *  another.  There WAS a relaxation pass here, but four things stopped it ever
 *  actually holding two bodies apart:
 *
 *    1. It ran a single sweep per frame and applied every pair's correction
 *       from the same starting positions, so in a knot of three or more people
 *       the pushes largely cancelled.  With the correction also capped at
 *       ~0.07 units, climbing out of a deep overlap took a dozen frames - and
 *       every one of them was drawn with the bodies inside each other.
 *
 *    2. Every correction was along the line joining the two centres.  When one
 *       villager walks up behind another - and they do: the shoppers share
 *       verge staggers ((i % 5) * 0.85), and walkNode() finishes each candidate
 *       with keepOffRoad(), which collapses different staggers onto the same
 *       verge boundary - that line points along the direction of travel.  The
 *       follower was shoved straight backwards, spent the same distance walking
 *       forwards again next frame, and so ground through the leader while
 *       dragging them off their spot.  Nobody ever stepped AROUND anybody.
 *
 *    3. Customer::advanceAlongPath() snaps a body onto a waypoint from up to
 *       0.45 units away without looking at who is already standing there, so
 *       two people bound for a shared waypoint can be placed exactly
 *       coincident - and the coincident branch then always shoved along world
 *       +X, whichever way the two were facing.
 *
 *    4. The children were separated BEFORE updateKidsAndFootball() moved them,
 *       so their half of the pass ran on the previous frame's positions and was
 *       undone the moment it finished.  At dusk all five walk to the same three
 *       doorway points, which is exactly when it showed.
 *
 *  This solver replaces that pass.  Adults and children go into one array so
 *  every pair obeys one rule; it relaxes several times per frame and applies
 *  each correction in place, so the next pair tested sees it and knots come
 *  apart instead of cancelling; it adds a sideways component so a body walking
 *  into another's back passes them rather than pushing them along; and the
 *  per-frame cap scales with the depth of the overlap, so a pair the waypoint
 *  snap dropped on top of each other is separated at once while a shoulder
 *  brush stays gentle.
 * ==========================================================================*/

/* Body radii.  Adult + adult comes to the 1.55 the old pass used shoulder to
 * shoulder, child + child to its 1.15, and an adult and a child to 1.35. */
const float kAdultHalf = 0.775f;
const float kChildHalf = 0.575f;

/* Share of a correction each kind of body absorbs.  A child is lighter and
 * gives way to an adult about two to one - the 0.65 / 1.35 split the old
 * adult-vs-child branch used, written as a weight so one rule now covers
 * adult/adult and child/child as well. */
const float kAdultGive = 0.65f;
const float kChildGive = 1.35f;

/* One person inside the solver. */
struct Body
{
    Vec3  start;      /* where they stood when the solver was handed them   */
    float x, z;       /* working position                                   */
    float vx, vz;     /* travel this frame: a rear-end, or a shoulder brush? */
    float half;       /* body radius                                        */
    float give;       /* share of a correction this body absorbs            */
    int   kid;        /* -1 for a customer, otherwise the child's index      */
    int   who;        /* index into mCustomers when kid < 0                  */
};

/* Shorten one correction to `step` without turning it. */
void clampStep(float& dx, float& dz, float step)
{
    const float m = std::sqrt(dx * dx + dz * dz);
    if (m > step && m > 1.0e-9f)
    {
        dx *= step / m;
        dz *= step / m;
    }
}

/* Where everybody stood before they walked this frame.  Travel is the only way
 * to tell a villager walking into somebody's back from two people standing
 * close, and that decides which way round the pair passes. */
std::vector<Vec3> gPrevCust;
Vec3 gPrevKid[kKidCount];
bool gPrevCustValid = false;
bool gPrevKidValid  = false;

/* --------------------------------------------------------------------------
 *  Take the snapshot.  Called once a frame from updateScene(), BEFORE either
 *  the customers or the children have moved.
 * ------------------------------------------------------------------------ */
void beginCrowdFrame(const std::vector<Customer>& people)
{
    gPrevCustValid = (gPrevCust.size() == people.size());
    if (!gPrevCustValid) gPrevCust.assign(people.size(), Vec3());

    for (std::size_t i = 0; i < people.size(); ++i)
        gPrevCust[i] = people[i].pos();

    gPrevKidValid = gKidsReady;
    if (gKidsReady)
        for (int i = 0; i < kKidCount; ++i)
            gPrevKid[i] = Vec3(gKids[i].x, 0.0f, gKids[i].z);
}

/* --------------------------------------------------------------------------
 *  Push every overlapping pair of people apart.
 *
 *  capScale scales the per-frame movement budget: the pass straight after the
 *  walk gets the whole of it, the short tidy-up after the road / stall / river
 *  clamps gets a fraction, since it only has to undo what those clamps broke.
 * ------------------------------------------------------------------------ */
void separateCrowd(std::vector<Customer>& people, float dt, float capScale)
{
    if (dt <= 0.0f) return;

    /* Nobody on foot covers more than this in one frame, so anything longer
     * was a jump, not a walk: a customer respawning at the path origin, or the
     * waypoint snap.  Clamping the length rather than throwing the travel away
     * keeps the direction, which is still along the path. */
    const float kTravelMax = std::max(0.02f, dt * 5.0f);

    /* ---- 1. gather everybody on foot into one array -------------------- */
    std::vector<Body> crowd;
    crowd.reserve(people.size() + static_cast<std::size_t>(kKidCount));

    for (std::size_t i = 0; i < people.size(); ++i)
    {
        if (!people[i].visible()) continue;

        const Vec3 p = people[i].pos();

        Body one;
        one.start = p;
        one.x     = p.x;
        one.z     = p.z;
        one.vx    = 0.0f;
        one.vz    = 0.0f;
        one.half  = kAdultHalf;
        one.give  = kAdultGive;
        one.kid   = -1;
        one.who   = static_cast<int>(i);

        if (gPrevCustValid)
        {
            float tx = p.x - gPrevCust[i].x;
            float tz = p.z - gPrevCust[i].z;
            const float t = std::sqrt(tx * tx + tz * tz);
            if (t > kTravelMax) { tx *= kTravelMax / t; tz *= kTravelMax / t; }
            one.vx = tx;
            one.vz = tz;
        }

        crowd.push_back(one);
    }

    if (kidsOutdoors())
        for (int k = 0; k < kKidCount; ++k)
        {
            const Vec3 p = kidPos(k);

            Body one;
            one.start = p;
            one.x     = p.x;
            one.z     = p.z;
            one.vx    = 0.0f;
            one.vz    = 0.0f;
            one.half  = kChildHalf;
            one.give  = kChildGive;
            one.kid   = k;
            one.who   = -1;              /* not one of the customers */

            if (gPrevKidValid)
            {
                float tx = p.x - gPrevKid[k].x;
                float tz = p.z - gPrevKid[k].z;
                const float t = std::sqrt(tx * tx + tz * tz);
                if (t > kTravelMax) { tx *= kTravelMax / t; tz *= kTravelMax / t; }
                one.vx = tx;
                one.vz = tz;
            }

            crowd.push_back(one);
        }

    if (crowd.size() < 2) return;

    /* ---- 2. relax ------------------------------------------------------ *
     * Several sweeps, each correction applied in place so the pairs tested
     * after it can see it.  That is what lets a knot of four people at a
     * shared waypoint open out, instead of every pair undoing its neighbour.
     *
     * The sweep count is set by the longest chain of people who can end up
     * pressed against each other, not by the size of the crowd: a correction
     * has to travel from one end of the chain to the other, one sweep per
     * link.  The five children file out of the house doorway in a line, which
     * is the longest chain the scene produces, and at four sweeps the far end
     * of it was still being left about 0.2 units inside its neighbour.  With
     * nineteen bodies at the very most this is a few hundred distance tests a
     * frame, so there is no reason to be stingy. */
    const int   kSweeps   = 8;
    const float kRelax    = 0.70f;   /* of the remaining overlap, per sweep */

    /* Sideways strength.  Only a pair actually closing on each other gets any:
     * two people walking abreast are simply eased apart, while one walking
     * into another's back is steered round them. */
    const float kSidestep = 1.10f;

    for (int sweep = 0; sweep < kSweeps; ++sweep)
    {
        for (std::size_t a = 0; a + 1 < crowd.size(); ++a)
        for (std::size_t b = a + 1; b < crowd.size(); ++b)
        {
            Body& A = crowd[a];
            Body& B = crowd[b];

            const float want = A.half + B.half;

            float dx = B.x - A.x;
            float dz = B.z - A.z;
            float d2 = dx * dx + dz * dz;
            if (d2 >= want * want) continue;

            float d = std::sqrt(d2);
            float nx, nz;

            if (d < 1.0e-4f)
            {
                /* Dropped exactly on top of each other by the waypoint snap.
                 * The direction is fixed by the pair, so the choice is the same
                 * every frame and the two do not jitter about it; the distance
                 * counts as zero, so the whole of the overlap gets worked out
                 * rather than a fraction of it. */
                const int   slot = static_cast<int>(a * 7 + b * 13) % 8;
                const float ang  = static_cast<float>(slot) * 0.7853982f;
                nx = std::cos(ang);
                nz = std::sin(ang);
                d  = 0.0f;
            }
            else
            {
                nx = dx / d;
                nz = dz / d;
            }

            const float overlap = want - d;

            /* How much of this is one body driving into the other rather than
             * the two merely standing close: the closing speed along the line
             * between them, as a fraction of one full stride. */
            const float rvx = A.vx - B.vx;
            const float rvz = A.vz - B.vz;

            float press = (rvx * nx + rvz * nz) / kTravelMax;
            if (press < 0.0f) press = 0.0f;      /* already drawing apart */
            if (press > 1.0f) press = 1.0f;

            /* Which way round to pass.  A steps to whichever side it is
             * already drifting toward relative to B, and B steps to the other,
             * so the pair opens out instead of one shoving the other along its
             * own line of travel. */
            const float tx   = -nz;
            const float tz   =  nx;
            const float lead = rvx * tx + rvz * tz;

            float sign;
            if (std::fabs(lead) > 1.0e-5f) sign = (lead > 0.0f) ? 1.0f : -1.0f;
            else                           sign = ((a + b) & 1) ? 1.0f : -1.0f;

            const float pushN = overlap * kRelax;
            const float pushT = overlap * press * kSidestep;

            /* A moves along -c, B along +c: the normal term separates them,
             * the sideways term sends them past each other. */
            const float cx = nx * pushN - sign * tx * pushT;
            const float cz = nz * pushN - sign * tz * pushT;

            const float sum = A.give + B.give;
            const float sa  = A.give / sum;
            const float sb  = B.give / sum;

            /* How far either body may be moved by this one correction.  It
             * grows with the depth of the overlap, so a pair the waypoint snap
             * left inside each other comes apart at once while a shoulder
             * brush stays a gentle drift.  Each body's share is shortened on
             * its own and nothing is rescaled afterwards - rescaling a whole
             * frame's correction to a fixed length is what used to drop two
             * coincident bodies back onto the very same point. */
            float step = dt * 6.0f * capScale;
            if (overlap * 0.35f * capScale > step) step = overlap * 0.35f * capScale;

            float ax = -cx * sa, az = -cz * sa;
            float bx =  cx * sb, bz =  cz * sb;

            clampStep(ax, az, step);
            clampStep(bx, bz, step);

            A.x += ax;  A.z += az;
            B.x += bx;  B.z += bz;
        }
    }

    /* ---- 3. hand the corrections back ---------------------------------- *
     * Exactly what the sweeps worked out: every correction was already limited
     * as it was applied, and each one is proportional to the overlap that is
     * left, so the solver settles at "just touching" and can never push a pair
     * further apart than that. */
    for (std::size_t i = 0; i < crowd.size(); ++i)
    {
        const Body& one = crowd[i];

        const float dx = one.x - one.start.x;
        const float dz = one.z - one.start.z;

        if (dx * dx + dz * dz < 1.0e-12f) continue;

        if (one.kid < 0) people[static_cast<std::size_t>(one.who)].nudge(dx, dz);
        else             kidNudge(one.kid, dx, dz);
    }
}

/* ==========================================================================
 *  Nearest spot clear of the fence timber
 *  --------------------------------------------------------------------------
 *  The lane router already keeps the walking lanes off the runs, but the crowd
 *  relaxation and the road clamp both move bodies afterwards with no idea the
 *  timber is there, and a few tenths of a unit is all it takes for a shoulder to
 *  finish up inside a gatepost.  Same safety net pushOutOfStalls() is for the
 *  canopies, and it treats an open gate bay as the gap it is.
 * ==========================================================================*/
Vec3 pushOutOfFences(const Vec3& p, float body)
{
    Vec3 out = p;

    const float want = body + 0.31f;        /* half a 0.62 post */

    for (int i = 0; i < kFenceCount; ++i)
    {
        const FenceRun& r = kFences[i];

        const int  bays  = (r.posts > 1) ? r.posts - 1 : 1;
        const bool gated = (r.gateT1 > r.gateT0);

        /* the pieces of this run that actually carry rails */
        Vec3 ends[2][2];
        int  n = 0;

        if (!gated)
        {
            ends[n][0] = r.a; ends[n][1] = r.b; ++n;
        }
        else
        {
            const int b0 = static_cast<int>(r.gateT0 * bays + 0.5f);
            const int b1 = static_cast<int>(r.gateT1 * bays + 0.5f);

            if (b0 >= 1)
            { ends[n][0] = r.a;                ends[n][1] = fencePostAt(r, b0); ++n; }
            if (b1 <= bays - 1)
            { ends[n][0] = fencePostAt(r, b1); ends[n][1] = r.b;                ++n; }
        }

        for (int k = 0; k < n; ++k)
        {
            const Vec3& a = ends[k][0];
            const Vec3& b = ends[k][1];

            const float ex = b.x - a.x;
            const float ez = b.z - a.z;
            const float l2 = ex * ex + ez * ez;
            if (l2 < 1.0e-9f) continue;

            float t = ((out.x - a.x) * ex + (out.z - a.z) * ez) / l2;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            const float cx = a.x + ex * t;
            const float cz = a.z + ez * t;

            float dx = out.x - cx;
            float dz = out.z - cz;
            float d  = std::sqrt(dx * dx + dz * dz);
            if (d >= want) continue;

            if (d < 1.0e-4f)
            {
                /* standing exactly on the line: leave along its normal */
                const float l = std::sqrt(l2);
                dx = ez / l;
                dz = -ex / l;
                d  = 1.0f;
            }

            out.x = cx + (dx / d) * want;
            out.z = cz + (dz / d) * want;
        }
    }
    return out;
}

/* ==========================================================================
 *  Nearest spot clear of every cart actually on the road
 *  --------------------------------------------------------------------------
 *  The corridor above is a tube of a per-sample radius around the road centre
 *  line, and each sample's radius is charged by whichever cart corner came
 *  nearest to THAT sample.  Through the sharp bend at the market end, the
 *  sample a swinging corner is charged to is not the sample a villager beside
 *  it is measured from, so the tube under-covers exactly where a 13-unit cart
 *  sweeps widest.  That is where the worst measured approach - a body centre
 *  0.608 units from a cart's flank, well inside the 0.99 the body occupies -
 *  was happening: a horse and cart driving through somebody's arm.
 *
 *  So the carts are tested directly as well, as the oriented boxes they are
 *  drawn as.  The corridor is what stops people walking down the lane; this is
 *  the guarantee that nothing overlaps when they do end up close.
 * ==========================================================================*/

/* Cart footprint in its own frame, facing +Z.  kTailBehind is private to
 * Carriage.cpp, so the figure is repeated here exactly as buildSweptEnvelope()
 * above already repeats it - if the cart body changes, both must follow. */
const float kCartTail = 4.30f;

Vec3 pushOutOfCarts(const Vec3& p, float body,
                    const std::vector<Carriage>& carts)
{
    Vec3 out = p;

    for (std::size_t i = 0; i < carts.size(); ++i)
    {
        if (!carts[i].active()) continue;

        const Vec3 c = carts[i].worldPos();
        const Vec3 f = carts[i].forwardDir();

        const float dx = out.x - c.x;
        const float dz = out.z - c.z;

        /* Same frame pedestrianGap() and buildSweptEnvelope() use: `along` up
         * the cart, `across` along its right vector (f.z, -f.x). */
        const float along  = dx * f.x + dz * f.z;
        const float across = dx * f.z - dz * f.x;

        const float front = Carriage::noseAhead() + body;   /* muzzle    */
        const float back  = -kCartTail - body;              /* rear board */
        const float flank = kCartHalfBody + body;

        if (along >= front || along <= back)  continue;
        if (std::fabs(across) >= flank)       continue;

        /* Inside the box, so leave it by the nearest face.  The box is 13 units
         * long and barely 4 wide, so the nearest way out is nearly always
         * sideways - which is also the way that takes somebody off the
         * carriageway rather than further along it. */
        const float outFront = front - along;
        const float outBack  = along - back;
        const float outSide  = flank - std::fabs(across);

        if (outSide <= outFront && outSide <= outBack)
        {
            const float sgn = (across >= 0.0f) ? 1.0f : -1.0f;
            out.x +=  f.z * outSide * sgn;
            out.z += -f.x * outSide * sgn;
        }
        else if (outFront <= outBack)
        {
            out.x += f.x * outFront;
            out.z += f.z * outFront;
        }
        else
        {
            out.x -= f.x * outBack;
            out.z -= f.z * outBack;
        }
    }
    return out;
}

/* Step `who` toward `target`, but no further than `cap` this frame, so a
 * correction reads as stepping smartly aside rather than as a teleport. */
void stepToward(float& dx, float& dz, const Vec3& from, const Vec3& target,
                float cap)
{
    dx = target.x - from.x;
    dz = target.z - from.z;

    const float m = std::sqrt(dx * dx + dz * dz);
    if (m > cap && m > 1.0e-9f)
    {
        dx *= cap / m;
        dz *= cap / m;
    }
}


/* ==========================================================================
 *  Audio backend
 *  --------------------------------------------------------------------------
 *  One continuous ambient bed (breeze + birdsong) looping forever.  There are
 *  no farmyard one-shots: the scene has no poultry in it, so a rooster or a
 *  clucking hen only ever sounded like it came from somewhere else.
 *
 *  The WAV lives in assets/ next to the sources, but CMake copies it beside
 *  the executable too, so both `./rural_market` (repo root) and
 *  `./build/rural_market` find it - hence the two-place lookup below.
 * ==========================================================================*/
const char* kAmbientName = "village_ambient.wav";

#if !defined(_WIN32)
pid_t gAudioPid = -1;                /* the looping ambient bed */
#endif

bool fileExists(const std::string& path)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    return f.good();
}

/* Return "assets/<name>" or "<name>", whichever exists; empty if neither. */
std::string findAsset(const char* name)
{
    std::string candidate = std::string("assets/") + name;
    if (fileExists(candidate)) return candidate;
    candidate = name;
    if (fileExists(candidate)) return candidate;
    return std::string();
}

#if !defined(_WIN32)
/* --------------------------------------------------------------------------
 *  Reap finished one-shot players.  Without this every clip would leave a
 *  zombie behind and a long session would slowly fill the process table -
 *  the same class of slow leak the HUD string builder used to be.
 *
 *  A *handler* is used rather than SIGCHLD = SIG_IGN on purpose: an ignored
 *  disposition survives exec(), which would break wait() inside the ambient
 *  loop's shell and spin it at 100% CPU.  Handlers are reset to SIG_DFL by
 *  exec(), so children are unaffected.
 * ------------------------------------------------------------------------ */
void reapChildren(int)
{
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
}

/* --------------------------------------------------------------------------
 *  atexit() handlers do NOT run when a process dies from a signal, so a plain
 *  `kill <pid>`, a Ctrl-C in the launching terminal, or a closed SSH session
 *  used to leave the setsid()-detached bed looping forever - with no window
 *  left to close and nothing obvious to kill.
 *
 *  This handler silences the audio, restores the default disposition and
 *  re-raises the same signal, so the process still dies of what killed it and
 *  the shell still reports the right status.  Everything it touches
 *  (kill/signal/raise) is async-signal-safe.
 * ------------------------------------------------------------------------ */
void stopAudioAndDie(int sig)
{
    sceneShutdownAudio();
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Shell fragment that plays one file once, on whatever player exists. */
std::string playerCommand(const std::string& path)
{
#if defined(__APPLE__)
    return "afplay '" + path + "'";
#else
    return "if command -v paplay >/dev/null 2>&1; then paplay '" + path +
           "'; else aplay -q '" + path + "'; fi";
#endif
}
#endif /* !_WIN32 */

} /* anonymous namespace */

/* ==========================================================================
 *  Audio init / shutdown
 * ==========================================================================*/
void Scene::initAudio()
{
    const std::string bed = findAsset(kAmbientName);
    if (bed.empty())
    {
        std::printf("[audio] '%s' not found in assets/ or the working "
                    "directory - running without the ambient bed.\n",
                    kAmbientName);
    }

#if !defined(_WIN32)
    /* Reap the ambient player if it ever exits on its own. */
    signal(SIGCHLD, reapChildren);

    /* ...and one covers every way of being killed that atexit() misses. */
    signal(SIGINT,  stopAudioAndDie);
    signal(SIGTERM, stopAudioAndDie);
    signal(SIGHUP,  stopAudioAndDie);
#endif

    if (bed.empty()) return;

#if defined(_WIN32)
    /* ---- Windows Multimedia API: looping asynchronous playback --------- */
    PlaySound(bed.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    std::printf("[audio] PlaySound loop started (winmm): %s\n", bed.c_str());

#else
    /* ---- POSIX: detached shell looping the platform's CLI player ------- */
    const std::string loop = "while :; do " + playerCommand(bed) + "; done";

    gAudioPid = fork();
    if (gAudioPid == 0)
    {
        setsid();                       /* own process group -> killable   */
        execlp("/bin/sh", "sh", "-c", loop.c_str(), (char*)NULL);
        _exit(127);
    }
    std::printf("[audio] ambient loop started (pid %d): %s\n",
                static_cast<int>(gAudioPid), bed.c_str());
#endif
}

void sceneShutdownAudio()
{
#if defined(_WIN32)
    PlaySound(NULL, NULL, SND_PURGE);
#else
    if (gAudioPid > 0)
    {
        kill(-gAudioPid, SIGTERM);      /* kill the whole process group */
        gAudioPid = -1;
    }
#endif
}

/* ==========================================================================
 *  Construction
 * ==========================================================================*/
Scene::Scene()
    : mTime(0.0f)
    , mSunSpin(0.0f)
    , mTimeOfDay(0.32f)        /* opens shortly after sunrise */
    , mPaused(false)
    , mBakedTint(1.0f, 1.0f, 1.0f)
    , mStaticList(0)
    , mStaticListValid(false)
{
}

void Scene::init()
{
    buildPathCenterline();
    buildStalls();
    buildCustomers();
    buildCarriages();
    buildProps();
    initAudio();
}

/* ==========================================================================
 *  Day / night clock
 *  --------------------------------------------------------------------------
 *  Everything the cycle does hangs off one number, mTimeOfDay, and the three
 *  little functions below.  Nothing else in the scene owns a lighting state:
 *  updateScene() pushes the result into gh::setSunlight() once per frame, and
 *  every voxel drawn after that is tinted by it.
 * ==========================================================================*/

/* -1 at midnight, 0 at sunrise / sunset, +1 at noon. */
float Scene::sunElevation() const
{
    return std::sin((mTimeOfDay - 0.25f) * 2.0f * static_cast<float>(M_PI));
}

/* 0 in the dark, 1 in full daylight.
 *
 * The band sits mostly BELOW the horizon on purpose.  Centring it on
 * elevation 0 would put the scene at half brightness exactly when the sun
 * touches the horizon, which kills the sunset: the warm light needs the
 * world still lit to land on.  Here the sun on the horizon is ~0.83 lit and
 * full dark only arrives once it is properly down, so dusk reads as a warm
 * evening that fades, rather than an abrupt grey. */
float Scene::daylight() const
{
    return smoothstep(-0.28f, 0.10f, sunElevation());
}

/* Peaks while the sun is on the horizon - the warm low-sun light. */
float Scene::goldenHour() const
{
    const float e = sunElevation() / 0.26f;
    return std::exp(-e * e) * daylight();
}

gh::Color Scene::sunlightTint() const
{
    const Color base = gh::mixColor(kLightNight, kLightDay, daylight());
    return gh::mixColor(base, kLightGolden, goldenHour() * 0.85f);
}

void Scene::skyColors(Color& top, Color& low) const
{
    const float day    = daylight();
    const float golden = goldenHour();

    top = gh::mixColor(kSkyTopNight, kSkyTop, day);
    low = gh::mixColor(kSkyLowNight, kSkyLow, day);

    /* The horizon takes more of the sunset than the zenith does, but the
     * zenith still has to take a good share of it - see kSkyTopDusk. */
    top = gh::mixColor(top, kSkyTopDusk, golden * 0.88f);
    low = gh::mixColor(low, kSkyLowDusk, golden * 0.95f);
}

/* --------------------------------------------------------------------------
 *  The shared sun / moon arc.  The moon runs exactly half a day behind the
 *  sun, so it rises as the sun sets and the sky is never empty.
 * ------------------------------------------------------------------------ */
void Scene::celestialPos(bool moon, float& x, float& y) const
{
    float u = (mTimeOfDay - 0.25f) * 2.0f * static_cast<float>(M_PI);
    if (moon) u += static_cast<float>(M_PI);

    x = kSkyArcCx - kSkyArcX * std::cos(u);
    y = kSkyArcY0 + kSkyArcY * std::sin(u);
}

void Scene::skipTimeOfDay(float fraction)
{
    mTimeOfDay += fraction;
    while (mTimeOfDay >= 1.0f) mTimeOfDay -= 1.0f;
    while (mTimeOfDay <  0.0f) mTimeOfDay += 1.0f;
}

const char* Scene::timeOfDayName() const
{
    if (mTimeOfDay < 0.20f) return "night";
    if (mTimeOfDay < 0.32f) return "sunrise";
    if (mTimeOfDay < 0.68f) return "day";
    if (mTimeOfDay < 0.80f) return "sunset";
    return "night";
}

/* ==========================================================================
 *  The winding dirt path
 *  --------------------------------------------------------------------------
 *  The road enters from the bottom-left foreground, climbs past the left
 *  vegetable stall, then swings hard right and runs along the BACK of the
 *  three right hand stalls (z ~ -10 .. -20, comfortably behind their rear
 *  edges).  It crosses the big tree at (50, -18) and runs all the way out
 *  through the RIGHT frame edge.  The same curve feeds the customer road
 *  nodes in buildCustomers().
 * ==========================================================================*/
void Scene::buildPathCenterline()
{
    Vec3  ctrl[8];
    float wid[8];

    ctrl[0] = Vec3(-19.0f, 0.0f,  54.0f); wid[0] = 15.0f;
    ctrl[1] = Vec3(-12.0f, 0.0f,  40.0f); wid[1] = 13.0f;
    ctrl[2] = Vec3( -4.0f, 0.0f,  27.0f); wid[2] = 11.0f;
    ctrl[3] = Vec3(  0.0f, 0.0f,  16.0f); wid[3] =  9.4f;
    ctrl[4] = Vec3( -1.0f, 0.0f,   2.0f); wid[4] =  8.2f;
    ctrl[5] = Vec3(  6.0f, 0.0f, -10.0f); wid[5] =  7.0f;
    ctrl[6] = Vec3( 26.0f, 0.0f, -14.0f); wid[6] =  5.8f;
    ctrl[7] = Vec3( 86.0f, 0.0f, -22.0f); wid[7] =  5.0f;

    mPathPts.clear();
    mPathWidth.clear();

    const int kSteps = 22;
    for (int seg = 0; seg < 7; ++seg)
    {
        const int i0 = (seg == 0) ? 0 : seg - 1;
        const int i1 = seg;
        const int i2 = seg + 1;
        const int i3 = (seg + 2 > 7) ? 7 : seg + 2;

        for (int s = 0; s < kSteps; ++s)
        {
            const float t = static_cast<float>(s) / kSteps;
            mPathPts.push_back(catmullRom(ctrl[i0], ctrl[i1],
                                          ctrl[i2], ctrl[i3], t));
            mPathWidth.push_back(lerpF(wid[i1], wid[i2], t));
        }
    }
    mPathPts.push_back(ctrl[7]);
    mPathWidth.push_back(wid[7]);

    /* Publish the sampled road so the river / bridge code and the pedestrian
     * backstop can test against the surface that is actually drawn, then find
     * the crossing.  buildRiver() reads gRoadPts, so this order matters. */
    gRoadPts  = mPathPts;
    gPathWidthForTrack = mPathWidth;
    gRoadHalf.clear();
    gRoadHalf.reserve(mPathWidth.size());
    for (std::size_t i = 0; i < mPathWidth.size(); ++i)
        gRoadHalf.push_back(mPathWidth[i] * 0.5f);

    /* Measure the ground the carts can actually cover before anything asks. */
    buildSweptEnvelope();

    buildRiver();
}

/* ==========================================================================
 *  Stalls - one market row along the road, every one of them drawn by the
 *  single drawGenericStall() renderer; only colour + produce shape differ.
 * ==========================================================================*/
void Scene::buildStalls()
{
    /* Placements live in kStallSpecs (see the stall-footprint block above) so
     * that the customer path planner tests the same boxes that get drawn. */
    mStalls.clear();
    mStalls.resize(static_cast<std::size_t>(kStallCount));

    for (int i = 0; i < kStallCount; ++i)
    {
        const StallSpec& s = kStallSpecs[i];
        mStalls[static_cast<std::size_t>(i)].init(
            s.type, s.x, s.z, s.rotY,
            Color(s.cr, s.cg, s.cb),
            Color(s.sr, s.sg, s.sb));
    }
}

/* ==========================================================================
 *  Customers - a crowd of fourteen, generated procedurally.  Everyone walks
 *  the VERGES (see the verge helpers above): the road centre is the carts'.
 *  Each shopper detours from the shoulder to the counter of one stall, then
 *  rejoins the shoulder and exits.  Four passers-by walk straight through
 *  the market on the far verge without ever stopping to shop.
 * ==========================================================================*/
void Scene::buildCustomers()
{
    /* Shirt / trouser palette cycled through the crowd. */
    static const float kShirt[14][3] =
    {
        {0.16f,0.48f,0.86f}, {0.78f,0.16f,0.14f}, {0.88f,0.88f,0.90f},
        {0.12f,0.55f,0.52f}, {0.92f,0.62f,0.14f}, {0.55f,0.28f,0.72f},
        {0.20f,0.62f,0.26f}, {0.90f,0.42f,0.62f}, {0.36f,0.34f,0.78f},
        {0.84f,0.78f,0.28f}, {0.16f,0.70f,0.85f}, {0.65f,0.45f,0.20f},
        {0.88f,0.30f,0.55f}, {0.30f,0.65f,0.45f}
    };
    static const float kTrouser[4][3] =
    {
        {0.14f,0.20f,0.42f}, {0.16f,0.17f,0.20f},
        {0.34f,0.24f,0.16f}, {0.28f,0.28f,0.32f}
    };

    const int kShoppers   = 10;   /* browse one stall, then leave */
    const int kPassersBy  = 4;    /* verge walkers, no shopping   */
    const int kCrowd      = kShoppers + kPassersBy;

    mCustomers.clear();
    mCustomers.resize(static_cast<std::size_t>(kCrowd));

    for (int i = 0; i < kShoppers; ++i)
    {
        /* Spread the crowd over the stalls. */
        const int  si   = i % kStallCount;
        const Vec3 spot = mStalls[static_cast<std::size_t>(si)].customerSpot();

        /* Stand-off point on the stall's own axis: the shopper walks in to
         * the counter head-on from here, and back out the same way. */
        const Vec3 approach = stallApproach(si, 4.6f);

        /* --------------------------------------------------------------
         *  Choose where to leave the verge.
         *
         *  The old code took whichever road node was nearest the counter and
         *  walked a straight line to it.  Nothing checked that line, so a
         *  shopper bound for a back-left stall walked through the canopy of
         *  the stall in front - the ghosting bug.
         *
         *  Now every candidate node is tested: the whole detour
         *  (verge -> approach -> counter, and back out to the next verge
         *  node) must be clear of every stall.  The nearest node that
         *  passes wins; ties are broken by distance so people still take
         *  the sensible turn-off rather than a scenic one.
         * ------------------------------------------------------------ */
        const float side    = sideOfRoadNearest(spot);
        const float stagger = (i % 5) * 0.85f;

        int   branch = -1;
        float best   = 1.0e9f;

        for (int n = 1; n <= 5; ++n)
        {
            const Vec3 v    = walkNode(n, side, stagger);
            const Vec3 vOut = walkNode((n + 1 < kRoadNodes) ? n + 1 : n,
                                            side, stagger);

            /* Routable, not merely straight-line clear: pushLeg can detour
             * round a canopy, so a branch that needs one sidestep is still a
             * perfectly good place to leave the lane. */
            if (!legRoutable(v, approach, side))        continue;
            if (!legRoutable(approach, spot, side))     continue;
            if (!legRoutable(spot, approach, side))     continue;
            if (!legRoutable(approach, vOut, side))     continue;

            const float d = gh::distXZ(kRoad[n], spot);
            if (d < best) { best = d; branch = n; }
        }

        /* Every stall in the scene is reachable, but if a future edit ever
         * boxes one in, fall back to walking past without shopping rather
         * than silently ghosting through a canopy. */
        const bool canShop = (branch >= 0);
        if (!canShop) branch = 2;

        /* Every leg goes through pushLeg, which detours round a canopy rather
         * than letting the shopper walk through it. */
        std::vector<Vec3> p;
        p.push_back(walkNode(0, side, (i % 4) * 0.9f));  /* off-camera */
        for (int n = 1; n <= branch; ++n)
            pushLeg(p, walkNode(n, side, stagger), side);

        int shopIdx = -1;
        if (canShop)
        {
            pushLeg(p, approach, side);            /* square up to the stall */
            pushLeg(p, spot, side);                /* SHOPPING waypoint      */
            shopIdx = static_cast<int>(p.size()) - 1;
            pushLeg(p, approach, side);            /* straight back out      */
        }

        for (int n = branch; n < kRoadNodes - 1; ++n)
            pushLeg(p, walkNode(n + 1, side, stagger), side);

        /* and on off frame, so the despawn happens out of shot */
        p.push_back(walkOffFrame(side, p.back()));

        CustomerLook look;
        look.shirt    = Color(kShirt[i][0], kShirt[i][1], kShirt[i][2]);
        look.trousers = Color(kTrouser[i % 4][0], kTrouser[i % 4][1],
                              kTrouser[i % 4][2]);
        look.scale    = 0.86f + 0.045f * static_cast<float>(i % 5);

        mCustomers[static_cast<std::size_t>(i)].init(
            i + 1, p, shopIdx, look,
            2.8f + 0.22f * static_cast<float>(i % 5),   /* speed       */
            static_cast<float>(i) * 1.5f);              /* spawn delay */
    }

    /* ---- passers-by: the far verge, no shop stop ------------------------ */
    for (int i = 0; i < kPassersBy; ++i)
    {
        std::vector<Vec3> p;
        const float stagger = 0.4f + 1.1f * static_cast<float>(i);
        p.push_back(walkNode(0, -1.0f, (i % 3) * 1.3f));
        for (int n = 1; n < kRoadNodes; ++n)
            pushLeg(p, walkNode(n, -1.0f, stagger), -1.0f);

        /* and on off frame, so the despawn happens out of shot */
        p.push_back(walkOffFrame(-1.0f, p.back()));

        CustomerLook look;
        look.shirt    = Color(kShirt[kShoppers + i][0],
                              kShirt[kShoppers + i][1],
                              kShirt[kShoppers + i][2]);
        look.trousers = Color(kTrouser[i % 4][0], kTrouser[i % 4][1],
                              kTrouser[i % 4][2]);
        look.scale    = 0.90f + 0.05f * static_cast<float>(i % 3);

        /* shopWaypoint -1: the customer never leaves the path, so the walk
         * runs straight to the exit and the customer respawns. */
        mCustomers[static_cast<std::size_t>(kShoppers + i)].init(
            kShoppers + i + 1, p, -1, look,
            2.4f + 0.30f * static_cast<float>(i),   /* speed              */
            1.5f + 5.0f * static_cast<float>(i));   /* spawn delay        */
    }
}

/* ==========================================================================
 *  Horse carts - two-way traffic on the road itself
 *  --------------------------------------------------------------------------
 *  Six carts, three per direction.  Carts in the same lane never overtake, so
 *  they are simply spaced out along their lane at start-up and
 *  Carriage::update() keeps them apart from there: each reads the cart in
 *  front and can never advance past it.  Opposing carts are held apart by the
 *  lane offset instead, so the two directions pass without interacting.
 * ==========================================================================*/
void Scene::buildCarriages()
{
    mTrack.build(mPathPts, mPathWidth);

    const int   kPerLane = 3;
    const float total    = mTrack.totalLength();

    mCarriages.clear();
    mCarriages.resize(static_cast<std::size_t>(kPerLane * 2));

    int id = 1;
    for (int laneIdx = 0; laneIdx < 2; ++laneIdx)
    {
        const int lane = (laneIdx == 0) ? 1 : -1;

        for (int i = 0; i < kPerLane; ++i)
        {
            /* Spread each lane evenly along its own direction of travel, the
             * leader furthest along.  The two lanes are deliberately offset
             * from each other so carts meet mid-road rather than all passing
             * at the same spot. */
            const float frac = (static_cast<float>(i) + 0.5f) /
                               static_cast<float>(kPerLane);
            const float startP = total * (1.0f - frac) +
                                 ((lane > 0) ? 0.0f : total * 0.14f);

            mCarriages[static_cast<std::size_t>(id - 1)].init(
                id, mTrack,
                4.6f + 0.55f * static_cast<float>(i % 3),   /* cruise speed */
                (startP > total) ? total : startP,
                lane);
            ++id;
        }
    }
}

/* ==========================================================================
 *  Sky props + scattered ground decoration
 * ==========================================================================*/
void Scene::buildProps()
{
    /* ---- clouds -------------------------------------------------------- */
    mClouds.clear();
    const float cx[6] = { -52.0f, -14.0f,  22.0f,  58.0f, -34.0f,  40.0f };
    const float cy[6] = {  40.0f,  46.0f,  38.0f,  43.0f,  33.0f,  31.0f };
    const float cz[6] = { -52.0f, -50.0f, -54.0f, -48.0f, -46.0f, -44.0f };
    const float cs[6] = {   1.30f,  1.55f,  1.10f,  1.35f,  0.90f,  1.00f };

    for (int i = 0; i < 6; ++i)
    {
        Cloud c;
        c.x     = cx[i];
        c.y     = cy[i];
        c.z     = cz[i];
        c.scale = cs[i];
        c.speed = 0.55f + 0.22f * (i % 3);
        c.shape = i % 3;
        mClouds.push_back(c);
    }

    /* ---- birds : 3D sine flight curves -------------------------------- */
    mBirds.clear();
    for (int i = 0; i < 5; ++i)
    {
        Bird b;
        b.t     = -30.0f - i * 9.0f;
        b.speed = 5.4f + 0.7f * i;
        b.baseY = 34.0f + 2.4f * i;
        b.amp   = 3.2f + 0.8f * i;
        b.zPos  = -34.0f - 2.5f * i;
        b.flap  = static_cast<float>(i) * 1.1f;
        b.scale = 1.15f - 0.08f * i;
        mBirds.push_back(b);
    }

    /* ---- grass tufts : scattered, but never on the dirt path ---------- */
    mTufts.clear();
    for (int i = 0; i < 220; ++i)
    {
        const float x = lerpF(-78.0f, 78.0f, hashNoise(i, 11));
        const float z = lerpF(-44.0f, 64.0f, hashNoise(i, 23));

        /* grass does not grow in the river, nor on its bare earth bank */
        if (inRiver(Vec3(x, 0.0f, z), kBankWidth)) continue;

        /* reject anything close to the path centre line */
        bool onPath = false;
        for (std::size_t k = 0; k < mPathPts.size(); k += 3)
        {
            const float dx = mPathPts[k].x - x;
            const float dz = mPathPts[k].z - z;
            if (dx * dx + dz * dz < (mPathWidth[k] * 0.62f) *
                                    (mPathWidth[k] * 0.62f))
            { onPath = true; break; }
        }
        if (!onPath)
            mTufts.push_back(Vec3(x, 0.0f, z));
    }

    /* ---- a few pebbles for foreground interest ------------------------- */
    mPebbles.clear();
    for (int i = 0; i < 26; ++i)
    {
        const float x = lerpF(-70.0f, 74.0f, hashNoise(i, 71));
        const float z = lerpF(  0.0f, 60.0f, hashNoise(i, 97));

        /* a pebble in the channel would float on the water surface */
        if (inRiver(Vec3(x, 0.0f, z), 0.6f)) continue;

        mPebbles.push_back(Vec3(x, 0.0f, z));
    }

    /* ---- stars : fixed, high, and well behind the sun and clouds ------- *
     * Kept above y = 20 so the background treeline never clips through the
     * field, and behind kSkyArcZ so the sun and moon always pass in front. */
    mStars.clear();
    for (int i = 0; i < 90; ++i)
    {
        Star s;
        s.x     = lerpF(-84.0f, 88.0f, hashNoise(i, 131));
        s.y     = lerpF( 20.0f, 46.0f, hashNoise(i, 157));
        s.z     = -64.0f;
        s.size  = 0.42f + 0.34f * hashNoise(i, 173);
        s.phase = hashNoise(i, 191) * 6.28f;
        mStars.push_back(s);
    }
}

/* ==========================================================================
 *  Traffic vs people
 *  --------------------------------------------------------------------------
 *  The two systems never spoke to each other.  Carts measured gaps only
 *  against other carts, and the crowd relaxation pass moved bodies with no
 *  idea that a road existed - so a shopper shoved sideways off the verge was
 *  driven straight through by the next cart along, and the cart never even
 *  slowed.  These two functions close that loop from both ends: the drivers
 *  are told how far away the nearest person in their path is, and anybody who
 *  ends up on the carriageway anyway is put back on the grass.
 * ==========================================================================*/
float Scene::pedestrianGap(const Carriage& c) const
{
    if (!c.active()) return 1.0e9f;

    const Vec3 p = c.worldPos();
    const Vec3 f = c.forwardDir();

    /* Half a cart plus half a body: anybody within this of the cart's centre
     * line is in its way, anybody outside it is standing clear.  Measured on
     * the real width of the avatar, so a driver brakes for an arm and not just
     * for a centre point. */
    const float kSweepHalf = kCartHalfBody + kWalkerBody;

    float best = 1.0e9f;

    /* One person, tested in the cart's own frame: `along` is how far ahead
     * they are, `across` is how far off the line of travel. */
    /* (a lambda would be tidier, but this file is C++98-flavoured C++11) */
    struct Local
    {
        static void test(const Vec3& who, const Vec3& p, const Vec3& f,
                         float sweepHalf, float& best)
        {
            const float dx = who.x - p.x;
            const float dz = who.z - p.z;

            const float along  = dx * f.x + dz * f.z;
            if (along <= 0.0f) return;                 /* behind the horse */

            const float across = dx * f.z - dz * f.x;
            if (std::fabs(across) > sweepHalf) return; /* standing clear   */

            /* Distance from the muzzle, not from the arc position. */
            const float gap = along - Carriage::noseAhead();
            if (gap < best) best = gap;
        }
    };

    for (std::size_t i = 0; i < mCustomers.size(); ++i)
    {
        if (!mCustomers[i].visible()) continue;
        Local::test(mCustomers[i].pos(), p, f, kSweepHalf, best);
    }

    if (kidsOutdoors())
        for (int i = 0; i < kidCount(); ++i)
            Local::test(kidPos(i), p, f, kSweepHalf, best);

    return best;
}

void Scene::keepCrowdOffRoad()
{
    /* The relaxation pass moves bodies with no idea what is around them, so a
     * run of shoves in one direction can put somebody in the road, in a stall
     * or in the river.  This puts them back.  Each correction is applied as a
     * fraction so it reads as stepping aside rather than as teleporting.
     *
     * Order matters: the road is the dangerous one, so it is resolved last -
     * whatever the other two do, nobody is left on the carriageway.  The carts
     * are resolved just before it, because pushing somebody away from the road
     * centre line also pushes them away from whatever is driving down it, so
     * the road step can only ever improve the cart clearance, never undo it. */
    for (std::size_t i = 0; i < mCustomers.size(); ++i)
    {
        if (!mCustomers[i].visible()) continue;

        Vec3 now = mCustomers[i].pos();

        /* out of the water first */
        if (inOpenWater(now, 0.0f))
        {
            const Vec3 dry = pushOutOfRiver(now);
            mCustomers[i].nudge((dry.x - now.x) * 0.40f,
                                (dry.z - now.z) * 0.40f);
            now = mCustomers[i].pos();
        }

        /* then out of any canopy */
        if (pointInAnyStall(now))
        {
            const Vec3 clear = pushOutOfStalls(now);
            mCustomers[i].nudge((clear.x - now.x) * 0.40f,
                                (clear.z - now.z) * 0.40f);
            now = mCustomers[i].pos();
        }

        /* out from under any horse and cart */
        {
            const Vec3 clear = pushOutOfCarts(now, kWalkerBody, mCarriages);
            if (clear.x != now.x || clear.z != now.z)
            {
                float dx, dz;
                stepToward(dx, dz, now, clear, 0.35f);
                mCustomers[i].nudge(dx, dz);
                now = mCustomers[i].pos();
            }
        }

        /* and out of the fence timber */
        {
            const Vec3 clear = pushOutOfFences(now, kWalkerBody);
            if (clear.x != now.x || clear.z != now.z)
            {
                float dx, dz;
                stepToward(dx, dz, now, clear, 0.35f);
                mCustomers[i].nudge(dx, dz);
                now = mCustomers[i].pos();
            }
        }

        /* and finally off the carriageway */
        if (inCartCorridor(now, 0.0f))
        {
            const Vec3 safe = keepOffRoad(now);
            mCustomers[i].nudge((safe.x - now.x) * 0.35f,
                                (safe.z - now.z) * 0.35f);
        }
    }

    /* The children, by the same rules. */
    if (!kidsOutdoors()) return;

    for (int i = 0; i < kidCount(); ++i)
    {
        Vec3 now = kidPos(i);

        if (inOpenWater(now, 0.0f))
        {
            const Vec3 dry = pushOutOfRiver(now);
            kidNudge(i, (dry.x - now.x) * 0.40f, (dry.z - now.z) * 0.40f);
            now = kidPos(i);
        }

        if (pointInAnyStall(now))
        {
            const Vec3 clear = pushOutOfStalls(now);
            kidNudge(i, (clear.x - now.x) * 0.40f, (clear.z - now.z) * 0.40f);
            now = kidPos(i);
        }

        {
            const Vec3 clear = pushOutOfCarts(now, kChildBody, mCarriages);
            if (clear.x != now.x || clear.z != now.z)
            {
                float dx, dz;
                stepToward(dx, dz, now, clear, 0.35f);
                kidNudge(i, dx, dz);
                now = kidPos(i);
            }
        }

        {
            const Vec3 clear = pushOutOfFences(now, kChildBody);
            if (clear.x != now.x || clear.z != now.z)
            {
                float dx, dz;
                stepToward(dx, dz, now, clear, 0.35f);
                kidNudge(i, dx, dz);
                now = kidPos(i);
            }
        }

        if (inCartCorridor(now, 0.0f))
        {
            const Vec3 safe = keepOffRoad(now);
            kidNudge(i, (safe.x - now.x) * 0.35f, (safe.z - now.z) * 0.35f);
        }
    }
}

/* ==========================================================================
 *  MASTER UPDATE
 * ==========================================================================*/
void Scene::updateScene(float dt)
{
    if (mPaused) return;

    mTime    += dt;

    /* ---- the day / night clock ---------------------------------------- */
    mTimeOfDay += dt / kDayLengthSec;
    while (mTimeOfDay >= 1.0f) mTimeOfDay -= 1.0f;

    /* ---- where everybody on foot stood before they walked -------------- *
     * The separation solver measures each person's travel over this frame to
     * work out which way round two people should pass, so the snapshot has to
     * be taken before anybody has moved. */
    beginCrowdFrame(mCustomers);

    /* ---- stalls: smoke particles + shopkeeper idle motion -------------- */
    for (std::size_t i = 0; i < mStalls.size(); ++i)
        mStalls[i].update(dt);

    /* ---- customers: EP3 state machine --------------------------------- */
    for (std::size_t i = 0; i < mCustomers.size(); ++i)
        mCustomers[i].update(dt);

    /* ---- children: pass-to-anyone football, home at dusk --------------- *
     * This has to run BEFORE the separation pass.  It used to sit right at the
     * end of the update, so the children were pushed out of the crowd using
     * the previous frame's positions and then walked wherever they liked
     * afterwards - their half of the pass was undone the moment it finished.
     * At dusk all five head for the same three doorway points, so that is
     * exactly when an adult could be seen walking through one. */
    updateKidsAndFootball(dt, 1.0f - daylight());

    /* ---- crowd separation --------------------------------------------- *
     * Everybody on foot - the shoppers, the verge walkers and the children -
     * is pushed out of everybody else.  See separateCrowd() for why path
     * following on its own lets them ghost through one another. */
    separateCrowd(mCustomers, dt, 1.0f);

    /* Whatever the relaxation just did, nobody is left standing in the road. */
    keepCrowdOffRoad();

    /* The road, stall and river clamps each move a body without looking at
     * where anybody else ended up, so two people shoved off the carriageway
     * can be set down on the same piece of verge.  One short relaxation to
     * open that back up, then clamp again - the road keeps the last word, so
     * nobody is left on the carriageway. */
    separateCrowd(mCustomers, dt, 0.45f);
    keepCrowdOffRoad();

    /* ---- horse carts: gap-limited road traffic ------------------------ *
     * Every cart reads the CURRENT positions of all the others, so the pass
     * is done against a snapshot: updating in place would let cart 0 move
     * first and then let cart 1 measure its gap against the new position,
     * which is how a queue ends up creeping into the cart in front.
     *
     * Carts also yield to people.  Nothing used to connect the two systems at
     * all: the drivers only ever looked at other drivers, so a villager who
     * ended up in the road - and the crowd relaxation below can shove one
     * there - was simply driven through.  The clear run to the nearest person
     * ahead is handed to update(), where it feeds the same ramp and the same
     * hard step clamp as the gap to the cart in front. */
    {
        const std::vector<Carriage> before = mCarriages;
        for (std::size_t i = 0; i < mCarriages.size(); ++i)
            mCarriages[i].update(dt, before, pedestrianGap(mCarriages[i]));
    }

    /* Re-admit one parked cart per frame, and only when the entry of ITS OWN
     * lane is clear - the gap rule cannot save a cart that spawns inside
     * another.  Progress is used rather than raw arc length so the check
     * means "near the start of your run" for both directions. */
    for (std::size_t i = 0; i < mCarriages.size(); ++i)
    {
        if (mCarriages[i].active()) continue;

        bool clear = true;
        for (std::size_t k = 0; k < mCarriages.size(); ++k)
        {
            if (k == i || !mCarriages[k].active())        continue;
            if (mCarriages[k].lane() != mCarriages[i].lane()) continue;
            if (mCarriages[k].progress() < Carriage::entryClearance())
            { clear = false; break; }
        }

        if (clear) { mCarriages[i].respawn(); break; }
    }

    /* ---- children: pass-to-anyone football, home at dusk --------------- *
     * Moved up ahead of the separation pass - see the call site there. */

    /* ---- the goat grazing in the corner -------------------------------- */
    updateGoat(dt, 1.0f - daylight());

    /* ---- the old boats drifting down the river ------------------------- */
    updateBoats(dt);

    /* ---- clouds drift right, wrapping around -------------------------- */
    for (std::size_t i = 0; i < mClouds.size(); ++i)
    {
        mClouds[i].x += mClouds[i].speed * dt;
        if (mClouds[i].x > 86.0f) mClouds[i].x = -86.0f;
    }

    /* ---- birds ride their sine curves --------------------------------- */
    for (std::size_t i = 0; i < mBirds.size(); ++i)
    {
        mBirds[i].t    += mBirds[i].speed * dt;
        mBirds[i].flap += dt * 9.0f;
        if (mBirds[i].t > 78.0f) mBirds[i].t = -78.0f;
    }
}

/* ==========================================================================
 *  Camera
 * ==========================================================================*/
void Scene::applyCamera() const
{
    /* Single fixed viewpoint: the elevated 3/4 view of colored.jpg.
     * No orbit, no zoom - the framing is a constant. */
    gluLookAt(4.0, 27.0, 70.0,      /* eye    */
              4.0,  6.0, -6.0,      /* target */
              0.0,  1.0,  0.0);     /* up     */
}

/* ==========================================================================
 *  MASTER DRAW
 * ==========================================================================*/
void Scene::drawScene(int screenW, int screenH)
{
    /* 0. hand this frame's sunlight to the primitives.  Everything drawn
     *    below is tinted by it, so the whole world darkens in one place. */
    gh::setSunlight(sunlightTint(), 1.0f - daylight());

    /* 1. sky gradient (2D overlay, no depth) */
    drawSky(screenW, screenH);

    /* 2. distant animated sky props */
    drawStars();
    drawSun();
    drawMoon();
    drawClouds();
    drawBirds();

    /* 3. cached static world: terrain, path, trees, fences, hedge */
    drawStaticWorld();

    /* 4. the stalls, each drawing its own vendor + goods */
    for (std::size_t i = 0; i < mStalls.size(); ++i)
        mStalls[i].draw();

    /* 5. the walking customers (EP3 / EP4)
     *
     * STATE_DESPAWN is skipped.  Customer::draw() renders that state by scaling
     * the whole avatar down by mFade over about half a second, which is fine
     * only for as long as it happens off camera - and it did not.  gluPerspective
     * fixes the VERTICAL field of view, so widening the window widens what the
     * frame takes in sideways: the market-side exit waypoint sits at screen x
     * 1.25 on the 4:3 window the scene opens at, but 0.94 at 16:9 and 0.72 at
     * 21:9.  Maximised on a widescreen monitor, ten of the fourteen villagers
     * were therefore walking to a point inside the frame, near the right-hand
     * wall, and shrinking away to nothing on screen.
     *
     * Not drawing the shrink at all is the fix that holds at every window
     * shape.  The exit waypoints in buildCustomers() are pushed off frame as
     * well, so the disappearance itself is out of shot too. */
    for (std::size_t i = 0; i < mCustomers.size(); ++i)
        if (mCustomers[i].state() != STATE_DESPAWN)
            mCustomers[i].draw();

    /* 6. the horse carts on the road */
    for (std::size_t i = 0; i < mCarriages.size(); ++i)
        mCarriages[i].draw();

    /* 7. the children's game (they walk home once it is dark) */
    drawKidsAndFootball();

    /* 8. the goat grazing in the corner */
    drawGoat();

    /* 9. the river's moving parts: ripples on the current, and the two old
     *    boats drifting down it.  Both are animated, so neither can live in
     *    the static display list the channel itself is baked into. */
    drawRiverRipples();
    drawBoats();
}

void Scene::drawSky(int screenW, int screenH) const
{
    Color top, low;
    skyColors(top, low);
    gh::drawVerticalGradient(screenW, screenH, top, low);
}

/* --------------------------------------------------------------------------
 *  Static world, compiled into a display list on first use.
 *
 *  The list bakes in whatever sunlight tint was current when it was compiled,
 *  so it has to be rebuilt as the sun moves.  Rebuilding every frame would
 *  throw the cache away entirely; instead the list is kept until the light
 *  has drifted more than kRelight in any channel.  That is a ~1% step in
 *  brightness - invisible on grass and dirt - and it means the list is
 *  rebuilt only during dawn and dusk, never through the still hours of noon
 *  or midnight.
 * ------------------------------------------------------------------------ */
void Scene::drawStaticWorld() const
{
    const float kRelight = 0.012f;
    const Color now      = gh::lightTint();

    if (mStaticListValid &&
        (std::fabs(now.r - mBakedTint.r) > kRelight ||
         std::fabs(now.g - mBakedTint.g) > kRelight ||
         std::fabs(now.b - mBakedTint.b) > kRelight))
    {
        mStaticListValid = false;
    }

    if (!mStaticListValid)
    {
        if (mStaticList == 0)
            mStaticList = glGenLists(1);

        mBakedTint = now;

        glNewList(mStaticList, GL_COMPILE_AND_EXECUTE);

            drawTerrain();
            drawBackgroundHedge();
            drawRiver();          /* channel first, then the road over it */
            drawDirtPath();
            drawBridge();
            drawGrassTufts();

            /* ---- trees ------------------------------------------------- *
             * All of these sit clear of the path centreline: the road swings
             * from (26,-14) out to (86,-22), so anything on the right has to
             * be pushed well behind that line or it grows out of the dirt. */
            drawTree(-46.0f,  -6.0f, 1.20f, 0);       /* big forked, left  */
            drawTree(-56.0f, -18.0f, 0.90f, 1);       /* smaller behind it */
            drawTree( 64.0f, -30.0f, 1.05f, 0);       /* right, past the river */
            drawTree( 76.0f, -32.0f, 0.80f, 1);

            /* A few more large ones filling the middle distance.  The one
             * that used to stand at (38,-36) is now at (28,-38): the river
             * runs through its old trunk. */
            drawTree(-42.0f, -34.0f, 1.30f, 0);
            drawTree( -8.0f, -32.0f, 1.20f, 0);
            drawTree( 20.0f, -32.0f, 1.25f, 0);
            drawTree( 28.0f, -38.0f, 1.15f, 0);

            /* ---- the two thatched cottages ----------------------------- */
            /* As in the reference: a big cottage with the smaller one right
             * beside it, eaves almost touching, the pair sitting behind the
             * stall row and left of the path (which has swung to positive x
             * by this depth).  Same rotY, so the two ridges stay parallel. */
            drawHouse(-32.0f, -14.0f, -8.0f, 1.55f, 2);   /* big, 2 windows */
            drawHouse(-16.5f, -18.5f, -8.0f, 1.10f, 1);   /* small, 1 window*/

            /* ---- post-and-rail fences ---------------------------------- *
             * Built from the kFences table, which the pedestrian lane router
             * reads as well, so no walking lane is ever laid through the
             * timber.  A run may leave one bay open as a field gate: the
             * market-side crowd leaves the frontage across the near-bank run,
             * so that bay carries no rails.
             *
             * The opening falls on the run's own post positions, so drawing the
             * run as two pieces puts every original post back in exactly its
             * old place at exactly its old spacing - including one either side
             * of the gap, which read as the gateposts.  The only thing removed
             * is the pair of rails across the opening. */
            for (int fi = 0; fi < kFenceCount; ++fi)
            {
                const FenceRun& r = kFences[fi];

                if (r.gateT1 <= r.gateT0)          /* an unbroken run */
                {
                    drawFenceRun(r.a, r.b, r.posts);
                    continue;
                }

                const int bays = (r.posts > 1) ? r.posts - 1 : 1;
                const int b0   = static_cast<int>(r.gateT0 * bays + 0.5f);
                const int b1   = static_cast<int>(r.gateT1 * bays + 0.5f);

                if (b0 >= 1)
                    drawFenceRun(r.a, fencePostAt(r, b0), b0 + 1);

                if (b1 <= bays - 1)
                    drawFenceRun(fencePostAt(r, b1), r.b, bays - b1 + 1);
            }

        glEndList();
        mStaticListValid = true;
    }
    else
    {
        glCallList(mStaticList);
    }
}

/* ==========================================================================
 *  Terrain - a grid of large grass blocks with subtle per-tile variation
 * ==========================================================================*/
void Scene::drawTerrain() const
{
    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);          /* no grid lines on the ground */

    const float tile = 18.0f;
    const int   nx   = static_cast<int>((kGroundMaxX - kGroundMinX) / tile) + 1;
    const int   nz   = static_cast<int>((kGroundMaxZ - kGroundMinZ) / tile) + 1;

    for (int ix = 0; ix < nx; ++ix)
        for (int iz = 0; iz < nz; ++iz)
        {
            const float x = kGroundMinX + tile * (ix + 0.5f);
            const float z = kGroundMinZ + tile * (iz + 0.5f);

            /* fade a little toward the horizon so depth reads clearly */
            const float depthT = std::min(1.0f,
                std::max(0.0f, (z - kGroundMinZ) / (kGroundMaxZ - kGroundMinZ)));
            Color c = gh::mixColor(gh::shade(kGrass, 0.93f), kGrass, depthT);

            /* tile-to-tile mottling */
            c = gh::shade(c, 0.96f + 0.08f * hashNoise(ix, iz));

            glPushMatrix();
                glTranslatef(x, -1.0f, z);
                gh::drawBlock(tile, 2.0f, tile, c);
            glPopMatrix();
        }

    /* a darker soil band under the front edge so the ground has thickness */
    glPushMatrix();
        glTranslatef(0.0f, -1.4f, kGroundMaxZ);
        gh::drawBlock(kGroundMaxX - kGroundMinX, 1.6f, 1.2f,
                      Color(0.42f, 0.28f, 0.14f));
    glPopMatrix();

    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  The winding dirt path - flat voxel tiles stamped along the spline
 * ==========================================================================*/
void Scene::drawDirtPath() const
{
    if (mPathPts.size() < 2) return;

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);          /* keep the path smooth */

    for (std::size_t i = 0; i + 1 < mPathPts.size(); ++i)
    {
        const Vec3& a = mPathPts[i];
        const Vec3& b = mPathPts[i + 1];

        const float dx  = b.x - a.x;
        const float dz  = b.z - a.z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-4f) continue;

        const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);
        const float w   = mPathWidth[i];

        /* The bridge deck is stacked over the crossing, so the dirt stops at
         * the bank: dirt drawn across the channel would read as a strip of
         * road floating over the water between the deck planks. */
        if (gBridgeReady &&
            static_cast<int>(i) >= gBridgeI0 &&
            static_cast<int>(i) <  gBridgeI1)
            continue;

        glPushMatrix();
            glTranslatef((a.x + b.x) * 0.5f, 0.0f, (a.z + b.z) * 0.5f);
            glRotatef(ang, 0.0f, 1.0f, 0.0f);

            /* darker worn edge slightly wider and lower */
            glPushMatrix();
                glTranslatef(0.0f, 0.06f, 0.0f);
                gh::drawBlock(w + 1.5f, 0.12f, len * 1.9f, kDirtEdge);
            glPopMatrix();

            /* main tan surface */
            glPushMatrix();
                glTranslatef(0.0f, 0.15f, 0.0f);
                const float mottle = 0.97f + 0.06f * hashNoise(
                    static_cast<int>(i), 5);
                gh::drawBlock(w, 0.14f, len * 1.9f,
                              gh::shade(kDirtPath, mottle));
            glPopMatrix();

            /* occasional lighter tread patch down the middle */
            if ((i % 7) == 0)
            {
                glPushMatrix();
                    glTranslatef(hashNoise(static_cast<int>(i), 3) * w * 0.4f -
                                 w * 0.2f, 0.23f, 0.0f);
                    gh::drawBlock(w * 0.26f, 0.06f, len * 1.6f,
                                  gh::shade(kDirtPath, 1.08f));
                glPopMatrix();
            }
        glPopMatrix();
    }

    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  The river and the bridge that crosses it
 * ==========================================================================*/
void Scene::drawRiver() const
{
    if (gRiverPts.size() < 2) return;

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    for (std::size_t i = 0; i + 1 < gRiverPts.size(); ++i)
    {
        const Vec3& a = gRiverPts[i];
        const Vec3& b = gRiverPts[i + 1];

        const float dx  = b.x - a.x;
        const float dz  = b.z - a.z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-4f) continue;

        const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);
        const float hw  = gRiverHalf[i];

        glPushMatrix();
            glTranslatef((a.x + b.x) * 0.5f, 0.0f, (a.z + b.z) * 0.5f);
            glRotatef(ang, 0.0f, 1.0f, 0.0f);

            /* bare earth bank, wider and slightly proud of the grass */
            glPushMatrix();
                glTranslatef(0.0f, kBankTopY, 0.0f);
                gh::drawBlock(hw * 2.0f + kBankWidth * 2.0f, 0.10f,
                              len * 1.9f, kRiverBank);
            glPopMatrix();

            /* the water itself */
            glPushMatrix();
                glTranslatef(0.0f, kWaterTopY, 0.0f);
                gh::drawBlock(hw * 2.0f, 0.10f, len * 1.9f,
                              gh::shade(kWater, 0.94f + 0.12f *
                                        hashNoise(static_cast<int>(i), 7)));
            glPopMatrix();
        glPopMatrix();
    }

    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  The timber bridge carrying the road over the water
 *  --------------------------------------------------------------------------
 *  Laid on the run of road samples buildRiver() found to be over the channel,
 *  so it always spans the crossing that is actually there.  The deck sits
 *  level with the dirt it carries (kDeckTopY matches the road's own top face),
 *  which is what lets the carts roll across and the villagers walk over it
 *  without any of them needing to know the bridge exists - they stay at y = 0
 *  and the planking is under their feet either way.
 * ==========================================================================*/
void Scene::drawBridge() const
{
    if (!gBridgeReady) return;

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    const Vec3& pa = gRoadPts[static_cast<std::size_t>(gBridgeI0)];
    const Vec3& pb = gRoadPts[static_cast<std::size_t>(gBridgeI1)];
    const float run = gh::distXZ(pa, pb) + 3.0f;   /* + abutment overlap */

    glPushMatrix();
        glTranslatef(gBridgeMid.x, 0.0f, gBridgeMid.z);
        glRotatef(std::atan2(gBridgeDir.x, gBridgeDir.z) * 180.0f /
                  static_cast<float>(M_PI), 0.0f, 1.0f, 0.0f);

        /* ---- the deck ------------------------------------------------- *
         * Planks laid ACROSS the road, the way a timber deck is actually
         * boarded, so the grain reads at right angles to the direction of
         * travel.  Each is shaded a little differently for old weathered
         * timber; the noise index comes from the plank number rather than
         * from the float offset, which would truncate to a couple of values
         * and band the deck.
         *
         * There is no trestle work under it on purpose.  The deck has to sit
         * level with the dirt it carries or the carts and the villagers - who
         * all stay at y = 0 - would sink into it, and a flush deck over a
         * surface-level channel leaves no gap underneath for a substructure
         * to be seen in.  What makes it read as a bridge is the planking, the
         * kerbs, the parapets and the stone abutments at each bank, which is
         * exactly how a low timber crossing reads from this distance. */
        const int planks = std::max(6, static_cast<int>(run / 1.15f));
        for (int p = 0; p < planks; ++p)
        {
            const float t = (static_cast<float>(p) + 0.5f) /
                            static_cast<float>(planks);
            const float along = (t - 0.5f) * run;

            glPushMatrix();
                glTranslatef(0.0f, kDeckTopY, along);
                gh::drawBlock(kDeckHalfW * 2.0f, 0.13f,
                              run / static_cast<float>(planks) * 0.86f,
                              gh::shade(kBridgeWood,
                                        0.90f + 0.16f * hashNoise(p, 9)));
            glPopMatrix();
        }

        /* the two stringers along the deck edges, a shade darker so the
         * boarding reads as sitting on a frame */
        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
                glTranslatef(side * kDeckHalfW * 0.97f, kDeckTopY - 0.01f,
                             0.0f);
                gh::drawBlock(0.42f, 0.17f, run,
                              gh::shade(kBridgeWood, 0.72f));
            glPopMatrix();
        }

        /* ---- kerbs and parapets --------------------------------------- */
        for (int side = -1; side <= 1; side += 2)
        {
            /* a raised kerb marking the footway off from the carriageway,
             * set just inside the walkers' line so it never blocks them */
            glPushMatrix();
                glTranslatef(side * (kFootwayOff - 1.15f), kDeckTopY + 0.11f,
                             0.0f);
                gh::drawBlock(0.26f, 0.14f, run * 0.98f,
                              gh::shade(kBridgeWood, 1.16f));
            glPopMatrix();

            /* post-and-rail parapet, open so the water stays visible
             * through it from this camera height */
            const int posts = std::max(4, static_cast<int>(run / 4.2f));
            for (int p = 0; p < posts; ++p)
            {
                const float t = (posts > 1)
                    ? (-1.0f + 2.0f * static_cast<float>(p) /
                                      static_cast<float>(posts - 1))
                    : 0.0f;

                glPushMatrix();
                    glTranslatef(side * kDeckHalfW, kDeckTopY + 0.68f,
                                 t * run * 0.47f);
                    gh::drawBlock(0.30f, 1.30f, 0.30f, kBridgeRail);
                glPopMatrix();
            }

            /* the handrail along the top of the posts */
            glPushMatrix();
                glTranslatef(side * kDeckHalfW, kDeckTopY + 1.36f, 0.0f);
                gh::drawBlock(0.38f, 0.22f, run * 0.98f,
                              gh::shade(kBridgeRail, 1.10f));
            glPopMatrix();

            /* a mid rail, so the parapet reads as built rather than as a
             * row of loose stakes */
            glPushMatrix();
                glTranslatef(side * kDeckHalfW, kDeckTopY + 0.76f, 0.0f);
                gh::drawBlock(0.24f, 0.16f, run * 0.98f, kBridgeRail);
            glPopMatrix();
        }

        /* ---- stone abutments where the deck meets each bank ------------ */
        for (int end = -1; end <= 1; end += 2)
        {
            glPushMatrix();
                glTranslatef(0.0f, kDeckTopY - 0.02f, end * run * 0.5f);
                gh::drawBlock(kDeckHalfW * 2.0f + 0.6f, 0.20f, 1.6f,
                              gh::shade(kStone, 0.92f));
            glPopMatrix();
        }
    glPopMatrix();

    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  Ripples drifting down the current
 *  --------------------------------------------------------------------------
 *  A handful of pale streaks carried downstream and recycled at the top, which
 *  is what stops the channel reading as a painted blue strip.  Animated off
 *  mTime, so like the boats these are drawn outside the static display list.
 * ==========================================================================*/
void Scene::drawRiverRipples() const
{
    if (gRiverPts.size() < 2) return;

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    for (int i = 0; i < 14; ++i)
    {
        /* each streak has its own start offset and speed */
        const float speed = 0.020f + 0.012f * hashNoise(i, 29);
        float t = hashNoise(i, 13) + mTime * speed;
        t -= std::floor(t);

        Vec3 p, dir;
        riverAt(t, p, dir);

        /* spread them across the channel rather than down the middle */
        float d, halfW;
        riverNearest(p, d, halfW);
        const float across = (hashNoise(i, 37) * 2.0f - 1.0f) * halfW * 0.62f;

        const Vec3  n(dir.z, 0.0f, -dir.x);
        const float yaw = std::atan2(dir.x, dir.z) * 180.0f /
                          static_cast<float>(M_PI);

        glPushMatrix();
            glTranslatef(p.x + n.x * across, kWaterTopY + 0.06f,
                         p.z + n.z * across);
            glRotatef(yaw, 0.0f, 1.0f, 0.0f);
            gh::drawBlock(1.5f + 1.4f * hashNoise(i, 53), 0.05f, 0.45f,
                          gh::mixColor(kWater, kWaterFoam, 0.55f));
        glPopMatrix();
    }

    gh::setOutlineEnabled(prevOutline);
}


/* ==========================================================================
 *  Tree : trunk cuboids + a cluster of plain spheres for the canopy.
 *  The old voxel-sphere / midpoint-disc rasterisers emitted several hundred
 *  shaded cubes per tree; three glutSolidSphere calls read the same.
 * ==========================================================================*/
void Scene::drawTree(float x, float z, float scale, int variant) const
{
    /* leaf blobs: x, y, z, radius - relative to the trunk base */
    static const float kCanopyA[3][4] = { {-4.4f, 15.0f,  0.6f, 4.6f},
                                          { 4.2f, 16.4f, -0.8f, 4.4f},
                                          { 0.0f, 19.4f,  0.0f, 4.8f} };
    static const float kCanopyB[3][4] = { { 0.0f, 14.0f,  0.0f, 5.0f},
                                          {-2.8f, 12.4f,  1.2f, 3.4f},
                                          { 2.9f, 12.8f, -1.0f, 3.4f} };

    const float (*canopy)[4] = (variant == 0) ? kCanopyA : kCanopyB;
    const float trunkH = (variant == 0) ? 12.0f : 10.4f;
    const float trunkW = (variant == 0) ?  2.1f :  1.8f;

    glPushMatrix();                       /* ---- tree frame ------------- */
        glTranslatef(x, 0.0f, z);
        glRotatef(hashNoise(static_cast<int>(x), static_cast<int>(z)) * 60.0f,
                  0.0f, 1.0f, 0.0f);
        glScalef(scale, scale, scale);

        /* ---- trunk + root flare ---------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, trunkH * 0.5f, 0.0f);
            gh::drawBlock(trunkW, trunkH, trunkW, kTrunk);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(0.0f, 0.50f, 0.0f);
            gh::drawBlock(trunkW * 1.42f, 1.00f, trunkW * 1.42f,
                          gh::shade(kTrunk, 0.86f));
        glPopMatrix();

        /* variant 0 forks into two angled branches */
        if (variant == 0)
        {
            const float lean[2] = { -34.0f, 30.0f };
            for (int b = 0; b < 2; ++b)
            {
                glPushMatrix();
                    glTranslatef(0.0f, 9.5f + b, 0.0f);
                    glRotatef(lean[b], 0.0f, 0.0f, 1.0f);
                    glTranslatef(0.0f, 2.5f, 0.0f);
                    gh::drawBlock(1.35f, 5.2f, 1.35f,
                                  gh::shade(kTrunk, 1.0f + 0.05f * b));
                glPopMatrix();
            }
        }

        /* ---- sphere canopy -------------------------------------------- */
        const float tint[3] = { 1.00f, 1.08f, 0.92f };
        for (int i = 0; i < 3; ++i)
        {
            const Color c = gh::shade((i == 2) ? kLeafDark : kLeafMid, tint[i]);
            gh::applyColor(c);
            glPushMatrix();
                glTranslatef(canopy[i][0], canopy[i][1], canopy[i][2]);
                glutSolidSphere(canopy[i][3], 12, 9);
            glPopMatrix();
        }

    glPopMatrix();                        /* ----------------------------- */
}

/* ==========================================================================
 *  Post-and-rail fence between two world points
 * ==========================================================================*/
void Scene::drawFenceRun(const Vec3& a, const Vec3& b, int posts) const
{
    if (posts < 2) posts = 2;

    const float dx  = b.x - a.x;
    const float dz  = b.z - a.z;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-3f) return;

    const float ang = static_cast<float>(std::atan2(dx, dz) * 180.0 / M_PI);

    const float postH = 3.4f;
    const float span  = len / (posts - 1);

    /* ---- posts ---------------------------------------------------------- */
    for (int i = 0; i < posts; ++i)
    {
        const float t = static_cast<float>(i) / (posts - 1);
        glPushMatrix();
            glTranslatef(a.x + dx * t, postH * 0.5f - 0.2f, a.z + dz * t);
            glRotatef(ang, 0.0f, 1.0f, 0.0f);
            gh::drawBlock(0.62f, postH, 0.62f, kFenceWood);
            /* Rounded cap, rasterised flat by the custom Midpoint Circle
             * Algorithm (EP1) - a filled disc of voxels in the XZ plane.
             * Cheap here: the whole fence lives in the static display list,
             * so these spans are rasterised once and then replayed. */
            gh::drawDiscMidpoint3D(0.0f, postH * 0.5f + 0.12f, 0.0f,
                                   2, 0.20f, gh::PLANE_XZ,
                                   gh::shade(kFenceWood, 1.14f));
        glPopMatrix();
    }

    /* ---- two horizontal rails ------------------------------------------ */
    for (int r = 0; r < 2; ++r)
    {
        const float y = (r == 0) ? 2.35f : 1.30f;
        for (int i = 0; i + 1 < posts; ++i)
        {
            const float t0 = static_cast<float>(i)     / (posts - 1);
            const float t1 = static_cast<float>(i + 1) / (posts - 1);
            const float mx = a.x + dx * (t0 + t1) * 0.5f;
            const float mz = a.z + dz * (t0 + t1) * 0.5f;

            glPushMatrix();
                glTranslatef(mx, y, mz);
                glRotatef(ang, 0.0f, 1.0f, 0.0f);
                gh::drawBlock(0.20f, 0.40f, span * 1.02f,
                              gh::shade(kFenceWood, (r == 0) ? 1.10f : 0.94f));
            glPopMatrix();
        }
    }
}

/* ==========================================================================
 *  Background hedge - two rows of overlapping green spheres along the horizon:
 *  a near row of bushes and a taller, darker row standing in for the far
 *  treeline.  The back row used to be a line of cuboids, which read as a row
 *  of green boxes on the skyline instead of foliage.
 * ==========================================================================*/
void Scene::drawBackgroundHedge() const
{
    for (int i = 0; i < 30; ++i)
    {
        const float n = hashNoise(i, 41);
        const float r = 3.4f + n * 2.2f;
        const Color c = gh::shade(kHedge, 0.90f + 0.16f * n);

        gh::applyColor(c);
        glPushMatrix();
            glTranslatef(-88.0f + i * 6.1f, r * 0.72f, -46.0f - n * 5.0f);
            glutSolidSphere(r, 10, 8);
        glPopMatrix();
    }

    /* far treeline silhouette behind the hedge - bigger, darker, rounder */
    for (int i = 0; i < 22; ++i)
    {
        const float n = hashNoise(i, 67);
        const float r = 6.0f + n * 3.4f;
        const Color c = gh::shade(kHedge, 0.72f + 0.10f * n);

        gh::applyColor(c);
        glPushMatrix();
            glTranslatef(-92.0f + i * 8.4f, r * 0.62f, -56.0f - n * 4.0f);
            glutSolidSphere(r, 10, 8);
        glPopMatrix();
    }
}

/* ==========================================================================
 *  Thatched village house
 *  --------------------------------------------------------------------------
 *  Modelled on the pair of cottages in reference/colored.jpg: a long cream
 *  front wall facing the market (+Z), a steep golden thatch roof whose ridge
 *  runs along X so the gable ends face +/-X, and a timber door set left of
 *  centre with barred windows filling the wall to its right.
 *
 *  Authored at "big house" size and uniformly scaled by the caller, so one
 *  function serves both cottages.  windows is the number of front windows
 *  (2 on the big house, 1 on the narrower one beside it).
 * ==========================================================================*/
void Scene::drawHouse(float x, float z, float rotY, float scale,
                      int windows) const
{
    /* Footprint is wider than it is deep, so the door wall reads as the long
     * face the way it does in the reference. */
    const float wallW   = 9.0f;    /* X span                        */
    const float wallH   = 4.6f;    /* eaves height above the plinth */
    const float wallD   = 6.6f;    /* Z span                        */
    const float roofH   = 3.5f;    /* ridge rise above the eaves    */
    const float eaves   = 1.25f;   /* thatch overhang on every side */
    const float plinthH = 0.40f;
    const float front   = wallD * 0.5f;   /* the +Z face we detail */

    glPushMatrix();
        glTranslatef(x, 0.0f, z);
        glRotatef(rotY, 0.0f, 1.0f, 0.0f);
        glScalef(scale, scale, scale);

        /* ---- grey stone plinth the walls stand on ---------------------- */
        glPushMatrix();
            glTranslatef(0.0f, plinthH * 0.5f, 0.0f);
            gh::drawBlock(wallW + 0.45f, plinthH, wallD + 0.45f, kPlinth);
        glPopMatrix();

        /* ---- cream rendered walls -------------------------------------- */
        glPushMatrix();
            glTranslatef(0.0f, plinthH + wallH * 0.5f, 0.0f);
            gh::drawBlock(wallW, wallH, wallD, kHouseWall);
        glPopMatrix();

        /* ---- thatch, ridge along X (gables face +/-X) ------------------- *
         * drawRoofPrism is authored with its ridge along Z, so a 90 deg Y
         * turn swaps its width/depth into our Z/X - hence the argument
         * order below.  The gable colour is the wall in roof shadow, which
         * is what gives the reference its grey triangular ends. */
        glPushMatrix();
            glTranslatef(0.0f, plinthH + wallH, 0.0f);
            glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
            gh::drawRoofPrism(0.0f, 0.0f, 0.0f,
                              wallD + eaves * 2.0f,   /* -> Z span */
                              roofH,
                              wallW + eaves * 2.0f,   /* -> X span */
                              kThatch, gh::shade(kHouseWall, 0.80f));
        glPopMatrix();

        /* ---- timber door, left of centre, with a stone step ------------- */
        const float doorX = -wallW * 0.5f + 1.85f;

        glPushMatrix();
            glTranslatef(doorX, plinthH + 1.30f, front + 0.02f);
            gh::drawBlock(1.50f, 2.60f, 0.16f, kHouseDoor);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(doorX, 0.16f, front + 0.55f);
            gh::drawBlock(2.40f, 0.32f, 1.10f, kPlinth);
        glPopMatrix();

        /* ---- barred windows filling the wall right of the door --------- */
        const float winX2[2] = { doorX + 2.55f, doorX + 5.15f };
        const float winY     = plinthH + 2.85f;
        const int   nWin     = (windows < 1) ? 1 : (windows > 2 ? 2 : windows);

        for (int i = 0; i < nWin; ++i)
        {
            glPushMatrix();
                glTranslatef(winX2[i], winY, front + 0.02f);

                /* Dark glazing in a thin frame.  No mullions: the cottages sit
                 * far enough back that window bars only read as noise. */
                gh::drawBlock(1.30f, 1.10f, 0.14f, kHouseTrim);

                /* After dusk somebody is home: the pane is drawn inside a
                 * lamp bracket so the light stays warm while the wall around
                 * it goes blue.  (This is inside the static display list -
                 * it is baked in and rebuilt with the rest as the sun moves.) */
                const float night = gh::nightFactor();
                glPushMatrix();
                    glTranslatef(0.0f, 0.0f, 0.06f);
                    if (night > 0.02f)
                    {
                        gh::beginLampLight();
                            gh::drawBlock(1.10f, 0.90f, 0.10f,
                                          gh::mixColor(kWindowDark,
                                                       Color(1.00f, 0.80f, 0.38f),
                                                       night));
                        gh::endLampLight();
                    }
                    else
                    {
                        gh::drawBlock(1.10f, 0.90f, 0.10f, kWindowDark);
                    }
                glPopMatrix();
            glPopMatrix();
        }

    glPopMatrix();
}

/* ==========================================================================
 *  Grass tufts + pebbles
 * ==========================================================================*/
void Scene::drawGrassTufts() const
{
    const Color tuft = gh::shade(kGrassDeep, 1.02f);

    for (std::size_t i = 0; i < mTufts.size(); ++i)
    {
        const Vec3& p = mTufts[i];
        const float n = hashNoise(static_cast<int>(i), 13);

        glPushMatrix();
            glTranslatef(p.x, 0.0f, p.z);
            glRotatef(n * 90.0f, 0.0f, 1.0f, 0.0f);

            /* a small fan of three blades */
            for (int b = -1; b <= 1; ++b)
            {
                glPushMatrix();
                    glTranslatef(b * 0.30f, 0.42f + 0.10f * (1 - std::abs(b)),
                                 0.0f);
                    glRotatef(b * -14.0f, 0.0f, 0.0f, 1.0f);  /* splay out */
                    gh::drawBlock(0.20f, 0.95f + 0.25f * n, 0.20f,
                                  gh::shade(tuft, 0.92f + 0.16f * n));
                glPopMatrix();
            }
        glPopMatrix();
    }

    for (std::size_t i = 0; i < mPebbles.size(); ++i)
    {
        const Vec3& p = mPebbles[i];
        const float n = hashNoise(static_cast<int>(i), 29);
        glPushMatrix();
            glTranslatef(p.x, 0.18f, p.z);
            gh::drawBlock(0.55f + n * 0.35f, 0.36f, 0.50f + n * 0.30f,
                          gh::shade(kStone, 0.90f + 0.2f * n));
        glPopMatrix();
    }
}

/* ==========================================================================
 *  Sun - EP1 showcase.
 *  A voxel sun that arcs across the sky on the day / night clock.  The disc
 *  is the custom Midpoint Circle algorithm and the eight rays are the custom
 *  3D Bresenham line, so both stay perfectly grid-aligned as it climbs.
 *
 *  Below the horizon it is simply not drawn - the moon has the sky then.
 * ==========================================================================*/
void Scene::drawSun() const
{
    if (sunElevation() < -0.16f) return;      /* set: the moon is up */

    /* Rays and disc are chunky on purpose - outlines would fight the shape. */
    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    float sx, sy;
    celestialPos(false, sx, sy);

    const float voxel = 1.05f;
    const int   discR = 6;      /* voxels */

    /* A low sun reddens; a high one is the ordinary yellow. */
    const float golden = goldenHour();
    const Color disc = gh::mixColor(kSunYellow, Color(1.00f, 0.46f, 0.16f),
                                    golden * 0.80f);
    const Color ray  = gh::mixColor(kSunRay,    Color(1.00f, 0.62f, 0.26f),
                                    golden * 0.80f);

    /* The sun is its own light source: it must not be dimmed by its own
     * tint, or it would go grey exactly when it is most visible. */
    gh::beginLampLight();

    glPushMatrix();
        glTranslatef(sx, sy, kSkyArcZ);

        /* ---- eight radiating rays, custom Bresenham -------------------- */
        for (int i = 0; i < 8; ++i)
        {
            const float angle = (i * 45.0f) * static_cast<float>(M_PI) / 180.0f;
            const float cosA  = std::cos(angle);
            const float sinA  = std::sin(angle);

            const int rx = static_cast<int>(cosA * 13.0f + (cosA > 0.0f ? 0.5f : -0.5f));
            const int ry = static_cast<int>(sinA * 13.0f + (sinA > 0.0f ? 0.5f : -0.5f));

            const int x0 = static_cast<int>(cosA * discR + (cosA > 0.0f ? 0.5f : -0.5f));
            const int y0 = static_cast<int>(sinA * discR + (sinA > 0.0f ? 0.5f : -0.5f));

            gh::drawLineBresenham3D(0.0f, 0.0f, 0.0f,
                                    x0, y0, 0,
                                    rx, ry, 0,
                                    voxel, ray);
        }

        /* ---- the disc itself, midpoint circle ------------------------ *
         * Nudged forward in Z so it wins the depth test against the rays. */
        gh::drawDiscMidpoint3D(0.0f, 0.0f, 0.6f,
                               discR, voxel, gh::PLANE_XY, disc);
    glPopMatrix();

    gh::endLampLight();
    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  Moon - the same EP1 rasterisers, half a day out of phase with the sun.
 *  A second, offset disc in the sky colour bites a crescent out of the face.
 * ==========================================================================*/
void Scene::drawMoon() const
{
    if (sunElevation() > 0.16f) return;       /* broad daylight */

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    float mx, my;
    celestialPos(true, mx, my);

    const float voxel = 0.92f;
    const int   discR = 5;

    /* Fade with the daylight so it does not pop in at dusk. */
    const float vis = 1.0f - daylight();

    Color top, low;
    skyColors(top, low);

    gh::beginLampLight();

    glPushMatrix();
        glTranslatef(mx, my, kSkyArcZ);

        gh::drawDiscMidpoint3D(0.0f, 0.0f, 0.0f, discR, voxel,
                               gh::PLANE_XY,
                               gh::mixColor(low, kMoonPale, vis));

        /* the shadow that makes it a crescent, in the sky's own colour */
        gh::drawDiscMidpoint3D(voxel * 3.4f, voxel * 1.0f, 0.4f,
                               discR - 1, voxel, gh::PLANE_XY, top);
    glPopMatrix();

    gh::endLampLight();
    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  Stars - a fixed field that fades in with the dark, each one twinkling on
 *  its own phase.  Blended, so they wash out gradually at dawn instead of
 *  switching off.
 * ==========================================================================*/
void Scene::drawStars() const
{
    const float vis = 1.0f - daylight();
    if (vis <= 0.02f) return;

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (std::size_t i = 0; i < mStars.size(); ++i)
    {
        const Star& s = mStars[i];

        const float tw = 0.62f + 0.38f * std::sin(mTime * 1.7f + s.phase);
        glColor4f(1.0f, 1.0f, 0.96f, vis * tw);

        glPushMatrix();
            glTranslatef(s.x, s.y, s.z);
            glutSolidCube(s.size);
        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  Voxel cloud clusters
 * ==========================================================================*/
void Scene::drawClouds() const
{
    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);          /* clouds read better unlined */

    for (std::size_t i = 0; i < mClouds.size(); ++i)
    {
        const Cloud& c = mClouds[i];

        glPushMatrix();
            glTranslatef(c.x, c.y, c.z);
            glScalef(c.scale, c.scale, c.scale);

            /* base slab */
            gh::drawBlock(13.0f, 2.6f, 5.0f, kCloudWhite);

            switch (c.shape)
            {
                case 0:
                    gh::draw3DCuboid(-2.6f, 1.9f, 0.0f, 6.4f, 2.6f, 4.6f,
                                     kCloudWhite.r, kCloudWhite.g,
                                     kCloudWhite.b);
                    gh::draw3DCuboid( 2.9f, 1.5f, 0.0f, 4.6f, 2.0f, 4.2f,
                                     kCloudWhite.r, kCloudWhite.g,
                                     kCloudWhite.b);
                    break;
                case 1:
                    gh::draw3DCuboid( 0.0f, 2.2f, 0.0f, 8.2f, 3.2f, 4.8f,
                                     kCloudWhite.r, kCloudWhite.g,
                                     kCloudWhite.b);
                    gh::draw3DCuboid(-3.4f, 1.4f, 0.0f, 4.4f, 2.0f, 4.2f,
                                     kCloudWhite.r, kCloudWhite.g,
                                     kCloudWhite.b);
                    gh::draw3DCuboid( 3.6f, 1.2f, 0.0f, 4.0f, 1.8f, 4.0f,
                                     kCloudWhite.r, kCloudWhite.g,
                                     kCloudWhite.b);
                    break;
                default:
                    gh::draw3DCuboid( 1.0f, 1.7f, 0.0f, 6.0f, 2.4f, 4.4f,
                                     kCloudWhite.r, kCloudWhite.g,
                                     kCloudWhite.b);
                    break;
            }
        glPopMatrix();
    }

    gh::setOutlineEnabled(prevOutline);
}

/* ==========================================================================
 *  Birds flying along 3D sine curves, with flapping wings
 * ==========================================================================*/
void Scene::drawBirds() const
{
    const Color feather(0.12f, 0.11f, 0.13f);

    const bool prevOutline = gh::outlineEnabled();
    gh::setOutlineEnabled(false);

    for (std::size_t i = 0; i < mBirds.size(); ++i)
    {
        const Bird& b = mBirds[i];

        /* ---- 3D sine flight curve ------------------------------------- */
        const float x = b.t;
        const float y = b.baseY + b.amp * std::sin(b.t * 0.11f);
        const float z = b.zPos  + 7.0f * std::sin(b.t * 0.06f);

        const float flap = std::sin(b.flap) * 42.0f;

        glPushMatrix();
            glTranslatef(x, y, z);
            glScalef(b.scale, b.scale, b.scale);
            /* bank into the turn */
            glRotatef(std::cos(b.t * 0.06f) * 12.0f, 0.0f, 0.0f, 1.0f);

            /* body */
            gh::drawBlock(1.5f, 0.55f, 0.60f, feather);

            /* two flapping wings - pivot at the shoulder (EP4 style) */
            for (int s = -1; s <= 1; s += 2)
            {
                glPushMatrix();
                    glTranslatef(0.0f, 0.12f, s * 0.28f);
                    glRotatef(s * flap, 1.0f, 0.0f, 0.0f);
                    glTranslatef(0.0f, 0.0f, s * 1.35f);
                    gh::drawBlock(1.0f, 0.16f, 2.6f, feather);
                glPopMatrix();
            }
            /* beak */
            glPushMatrix();
                glTranslatef(0.92f, 0.0f, 0.0f);
                gh::drawBlock(0.36f, 0.18f, 0.18f,
                              gh::shade(feather, 1.9f));
            glPopMatrix();
        glPopMatrix();
    }

    gh::setOutlineEnabled(prevOutline);
}