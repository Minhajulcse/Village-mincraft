/* ==========================================================================
 *  main.cpp
 *  --------------------------------------------------------------------------
 *  GLUT entry point for the "Rural Market" simplified Minecraft / voxel scene.
 *
 *  Responsibilities are kept deliberately thin - all geometry and animation
 *  work lives in Scene / Stall / Customer / GraphicsHelpers.  This file:
 *      -> creates the double buffered RGB + depth FreeGLUT window
 *      -> sets up the fixed function GL state and the perspective projection
 *      -> owns the single Scene instance
 *      -> drives the ~60 FPS animation loop (glutTimerFunc, 16 ms)
 *      -> translates a few keyboard shortcuts into pause and render
 *         style changes
 *
 *  The viewpoint is FIXED: there is no orbit and no zoom, so the framing
 *  always matches the reference still.
 * ==========================================================================*/
#include "Scene.h"
#include "GraphicsHelpers.h"    /* pulls in GL/GLU/GLUT for every platform */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

/* ==========================================================================
 *  File scope state
 *  --------------------------------------------------------------------------
 *  GLUT callbacks are plain C style functions, so the scene has to live here
 *  rather than inside main().
 * ==========================================================================*/
Scene gScene;

int  gWinW = 1280;              /* current framebuffer width  */
int  gWinH = 960;               /* current framebuffer height */

bool gOutlines = true;          /* black voxel outlines on / off ('o' key) */

/* ==========================================================================
 *  GL setup
 *  --------------------------------------------------------------------------
 *  NOTE: face culling is intentionally left DISABLED.  Several pieces of the
 *  scene (signs, cloth awnings, flat foliage / cloud slabs, midpoint circle
 *  spans) are single sided, so GL_CULL_FACE would punch holes in them.
 * ==========================================================================*/
void initGL()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);

    /* Sky blue - matches the low band of the Scene sky gradient, so any pixel
     * the overlay does not cover still reads as sky. */
    glClearColor(0.62f, 0.85f, 0.99f, 1.0f);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    /* Seed the std::rand based helpers used while populating the scene. */
    std::srand(static_cast<unsigned>(std::time(NULL)));

    /* A GL context exists by now, so display lists / geometry can be built. */
    gScene.init();
}

/* ==========================================================================
 *  Window callbacks
 * ==========================================================================*/

/* Viewport + perspective projection follow the window size. */
void reshape(int w, int h)
{
    if (h == 0) { h = 1; }

    gWinW = w;
    gWinH = h;

    glViewport(0, 0, w, h);

    const double aspect = static_cast<double>(w) / static_cast<double>(h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* Far plane is deliberately generous: the background hedge and the sun sit
     * far behind the market, well past 100 units from the fixed eye point. */
    gluPerspective(45.0, aspect, 0.5, 500.0);

    glMatrixMode(GL_MODELVIEW);
}

/* Clear, place the fixed camera, then hand the frame to the scene. */
void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* drawScene() paints the sky as a 2D overlay and then draws the 3D world
     * with whatever MODELVIEW matrix is current, so the camera goes first. */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gScene.applyCamera();               /* issues the gluLookAt */

    gScene.drawScene(gWinW, gWinH);

    glutSwapBuffers();
}

/* ==========================================================================
 *  Animation loop (~60 FPS)
 *  --------------------------------------------------------------------------
 *  Fixed 16 ms timer with a fixed time step, re-armed at the end of every
 *  tick so the loop keeps running for the life of the program.
 * ==========================================================================*/
void update(int value)
{
    (void)value;                        /* one timer id is enough here */

    const float dt = 0.016f;            /* fixed step, ~60 FPS */

    gScene.updateScene(dt);

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

/* ==========================================================================
 *  Keyboard - ASCII keys: quit plus the style / pause toggles
 * ==========================================================================*/
void keyboard(unsigned char key, int x, int y)
{
    (void)x;                            /* every action is position free */
    (void)y;

    switch (key)
    {
        case 27:                        /* ESC */
        case 'q':
        case 'Q':
            sceneShutdownAudio();       /* stop the ambient loop cleanly */
            std::exit(0);
            break;

        case 'o':                       /* voxel outlines on / off */
        case 'O':
            gOutlines = !gOutlines;
            gh::setOutlineEnabled(gOutlines);
            /* The static world is cached in a display list that baked in the
             * previous outline state, so it MUST be rebuilt. */
            gScene.invalidateStaticGeometry();
            break;

        case 'p':                       /* freeze / resume the animation */
        case 'P':
            gScene.togglePause();
            break;

        case 'n':                       /* jump a quarter day */
        case 'N':
            gScene.skipTimeOfDay(0.25f);
            /* The static world bakes the sunlight into its display list, so
             * a jump this large has to force a rebuild rather than wait for
             * the usual drift threshold. */
            gScene.invalidateStaticGeometry();
            std::printf("[time] %s\n", gScene.timeOfDayName());
            std::fflush(stdout);
            break;

        default:
            break;
    }

    glutPostRedisplay();
}

/* ==========================================================================
 *  Entry point - window creation, callback registration, timer start
 * ==========================================================================*/
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(gWinW, gWinH);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("Rural Market - Simplified Minecraft / Voxel 3D Scene");

    initGL();                           /* GL state + gScene.init() */

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);

    /* Kills the POSIX aplay/paplay fallback child on every other exit path. */
    std::atexit(sceneShutdownAudio);

    glutTimerFunc(16, update, 0);       /* start the ~60 FPS loop */

    std::printf("\n");
    std::printf("=========================================================\n");
    std::printf("  Rural Market - Voxel 3D Scene (OpenGL / FreeGLUT)\n");
    std::printf("=========================================================\n");
    std::printf("  Fixed camera      : no orbit, no zoom\n");
    std::printf("  O                 : toggle voxel outlines\n");
    std::printf("  P                 : pause / resume the animation\n");
    std::printf("  N                 : skip a quarter day (dawn/noon/dusk/night)\n");
    std::printf("  Q / ESC           : quit\n");
    std::printf("=========================================================\n\n");
    std::fflush(stdout);

    glutMainLoop();
    return 0;
}
