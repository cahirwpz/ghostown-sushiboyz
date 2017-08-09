#include "sushiboyz.h"
#include "2d.h"
#include "blitter.h"
#include "coplist.h"
#include "memory.h"
#include "io.h"
#include "ilbm.h"
#include "ffp.h"
#include "fx.h"
#include "3d.h"

#define WIDTH  320
#define HEIGHT 240
#define DEPTH  5

static Mesh3D *mesh;
static Object3D *ball;
static PaletteT *palette;
static BitmapT *window;
static CopInsT *bplptr[DEPTH];
static CopListT *cp;
static WORD active = 0;

static TrackT *man_x, *man_active;
static TrackT *obj_rot_x, *obj_rot_y, *obj_rot_z, *obj_scale, *obj_pos_x, *obj_pos_y;

typedef struct {
  WORD width, height;
  WORD current, count;
  UBYTE *frame[0];
} AnimSpanT;

static AnimSpanT *anim;

static void Load() {
  WORD i;

  palette = LoadPalette("09_running_pal.iff");
  anim = LoadFile("09_running_anim.bin", MEMF_PUBLIC);
  mesh = LoadMesh3D("09_ball.3d", SPFlt(40));
  CalculateEdges(mesh);

  Log("Animation has %ld frames %ld x %ld.\n", 
      (LONG)anim->count, (LONG)anim->width, (LONG)anim->height);

  for (i = 0; i < anim->count; i++)
    anim->frame[i] = (APTR)anim->frame[i] + (LONG)anim;

  man_x = TrackLookup(tracks, "running.man.x");
  man_active = TrackLookup(tracks, "running.man.active");

  obj_rot_x = TrackLookup(tracks, "running.obj.rotate.x");
  obj_rot_y = TrackLookup(tracks, "running.obj.rotate.y");
  obj_rot_z = TrackLookup(tracks, "running.obj.rotate.z");
  obj_pos_x = TrackLookup(tracks, "running.obj.x");
  obj_pos_y = TrackLookup(tracks, "running.obj.y");
  obj_scale = TrackLookup(tracks, "running.obj.scale");
}

static void UnLoad() {
  MemFree(anim);
  DeleteMesh3D(mesh);
  DeletePalette(palette);
}

static void Init() {
  static BitmapT recycled;

  window = &recycled;

  InitSharedBitmap(window, WIDTH, HEIGHT, DEPTH, screen0);
  /* borrow extra bitplane from second screen */
  window->depth++;
  window->planes[DEPTH] = screen1->planes[0];

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;
  BitmapClear(window);

  ball = NewObject3D(mesh);
  ball->translate.z = fx4i(-250);

  cp = NewCopList(100);
  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, bplptr, window, DEPTH);
  CopLoadPal(cp, palette, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  DeleteObject3D(ball);
  DeleteCopList(cp);
}

#define MULVERTEX1(D, E) {            \
  WORD t0 = (*v++) + y;               \
  WORD t1 = (*v++) + x;               \
  LONG t2 = (*v++) * z;               \
  v++;                                \
  D = ((t0 * t1 + t2 - xy) >> 4) + E; \
}

#define MULVERTEX2(D) {               \
  WORD t0 = (*v++) + y;               \
  WORD t1 = (*v++) + x;               \
  LONG t2 = (*v++) * z;               \
  WORD t3 = (*v++);                   \
  D = normfx(t0 * t1 + t2 - xy) + t3; \
}

static __regargs void TransformVertices(Object3D *object) {
  Matrix3D *M = &object->objectToWorld;
  WORD *v = (WORD *)M;
  WORD *src = (WORD *)object->mesh->vertex;
  WORD *dst = (WORD *)object->vertex;
  WORD n = object->mesh->vertices;

  LONG m0 = (M->x - normfx(M->m00 * M->m01)) << 8;
  LONG m1 = (M->y - normfx(M->m10 * M->m11)) << 8;
  /* WARNING! This modifies camera matrix! */
  M->z -= normfx(M->m20 * M->m21);

  /*
   * A = m00 * m01
   * B = m10 * m11
   * C = m20 * m21 
   * yx = y * x
   *
   * (m00 + y) * (m01 + x) + m02 * z - yx + (mx - A)
   * (m10 + y) * (m11 + x) + m12 * z - yx + (my - B)
   * (m20 + y) * (m21 + x) + m22 * z - yx + (mz - C)
   */

  while (--n >= 0) {
    WORD x = *src++;
    WORD y = *src++;
    WORD z = *src++;
    LONG xy = x * y;
    LONG xp, yp;
    WORD zp;

    pushl(v);
    MULVERTEX1(xp, m0);
    MULVERTEX1(yp, m1);
    MULVERTEX2(zp);
    popl(v);

    *dst++ = div16(xp, zp) + WIDTH / 2;  /* div(xp * 256, zp) */
    *dst++ = div16(yp, zp) + HEIGHT / 2; /* div(yp * 256, zp) */
    *dst++ = zp;

    src++;
    dst++;
  }
}

static __regargs void DrawObject(Object3D *object, APTR start) {
  WORD *edge = (WORD *)object->mesh->edge;
  Point3D *point = object->vertex;
  WORD n = object->mesh->edges;

  WaitBlitter();

  custom->bltafwm = -1;
  custom->bltalwm = -1;
  custom->bltadat = 0x8000;
  custom->bltbdat = 0xffff; /* Line texture pattern. */
  custom->bltcmod = WIDTH / 8;
  custom->bltdmod = WIDTH / 8;

  do {
    WORD *p0 = (APTR)point + *edge++;
    WORD *p1 = (APTR)point + *edge++;

    WORD x0 = *p0++, y0 = *p0++;
    WORD x1 = *p1++, y1 = *p1++;

    if (y0 > y1) {
      swapr(x0, x1);
      swapr(y0, y1);
    }

    {
      APTR data = start + (((y0 * 40) + (x0 >> 3)) & ~1);
      WORD dmax = x1 - x0;
      WORD dmin = y1 - y0;
      WORD derr;
      UWORD bltcon1 = LINE_SOLID;

      if (dmax < 0)
        dmax = -dmax;

      if (dmax >= dmin) {
        if (x0 >= x1)
          bltcon1 |= (AUL | SUD);
        else
          bltcon1 |= SUD;
      } else {
        if (x0 >= x1)
          bltcon1 |= SUL;
        swapr(dmax, dmin);
      }

      derr = 2 * dmin - dmax;
      if (derr < 0)
        bltcon1 |= SIGNFLAG;
      bltcon1 |= rorw(x0 & 15, 4);

      {
        UWORD bltcon0 = rorw(x0 & 15, 4) | LINE_OR;
        UWORD bltamod = derr - dmax;
        UWORD bltbmod = 2 * dmin;
        UWORD bltsize = (dmax << 6) + 66;
        APTR bltapt = (APTR)(LONG)derr;

        WaitBlitter();

        custom->bltcon0 = bltcon0;
        custom->bltcon1 = bltcon1;
        custom->bltamod = bltamod;
        custom->bltbmod = bltbmod;
        custom->bltapt = bltapt;
        custom->bltcpt = data;
        custom->bltdpt = data;
        custom->bltsize = bltsize;
      }
    }
  } while (--n > 0);
}

static void DrawSpans(UBYTE *bpl) {
  UBYTE *frame = anim->frame[anim->current];
  WORD xc = TrackValueGet(man_x, frameCount);
  WORD n = anim->height;
  WORD stride = window->bytesPerRow;

  WaitBlitter();

  while (--n >= 0) {
    WORD m = *frame++;

    while (--m >= 0) {
      WORD x = *frame++;
      x += xc;
      bset(bpl + (x >> 3), ~x);
    }

    bpl += stride;
  }

  anim->current++;

  if (anim->current >= anim->count)
    anim->current -= anim->count;
}

static inline void SwapBitplanes() {
  WORD n = DEPTH;

  WaitVBlank();

  while (--n >= 0) {
    WORD i = (active + n + 1 - DEPTH) % (DEPTH + 1);
    if (i < 0)
      i += DEPTH + 1;
    CopInsSet32(bplptr[n], window->planes[i]);
  }

  active = (active + 1) % (DEPTH + 1);
}

static void Render() {
  // PROFILE_BEGIN(running);
  BOOL running = TrackValueGet(man_active, frameCount);

  ball->rotate.x = TrackValueGet(obj_rot_x, frameCount) * 4;
  ball->rotate.y = TrackValueGet(obj_rot_y, frameCount) * 4;
  ball->rotate.z = TrackValueGet(obj_rot_z, frameCount) * 4;
  ball->scale.x = ball->scale.y = ball->scale.z =
    TrackValueGet(obj_scale, frameCount) * 4;
  ball->translate.x = TrackValueGet(obj_pos_x, frameCount);
  ball->translate.y = TrackValueGet(obj_pos_y, frameCount);

  BlitterClear(window, active);
  UpdateObjectTransformation(ball);
  if (running) {
    DrawSpans(window->planes[active]);
    BlitterFill(window, active);
  }
  TransformVertices(ball);
  DrawObject(ball, window->planes[active]);

  // PROFILE_END(running);
  SwapBitplanes();
}

EFFECT(RunningMan, Load, UnLoad, Init, Kill, Render, NULL);
