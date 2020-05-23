#include "sushiboyz.h"
#include "blitter.h"
#include "coplist.h"
#include "3d.h"
#include "fx.h"
#include "ffp.h"
#include "ilbm.h"
#include "sprite.h"
#include "interrupts.h"
#include "tasks.h"

#define WIDTH  256
#define HEIGHT 256
#define DEPTH  4

static PaletteT *palette[2], *activePal;
static Mesh3D *mesh[2];
static Object3D *object[2];
static CopListT *cp;
static CopInsT *bplptr[DEPTH], *pal;
static BitmapT *window0, *window1;
static BitmapT *buffer, *bamboo;
static SpriteT *sprite[2];

static TrackT *obj_rot_x, *obj_rot_y, *obj_rot_z, *obj_scale, *obj_pos_x, *obj_pos_y, *obj_active;
static TrackT *flash, *obj_palette;

static void Load() {
  mesh[0] = LoadMesh3D("08_codi.3d", SPFlt(384));
  mesh[1] = LoadMesh3D("08_kolczak2.3d", SPFlt(128));
  CalculateFaceNormals(mesh[0]);
  CalculateFaceNormals(mesh[1]);

  palette[0] = LoadPalette("08_flatshade_pal.iff");
  palette[1] = LoadPalette("08_flatshade_pal2.iff");
  activePal = palette[0];
  bamboo = LoadILBMCustom("08_bamboo.iff", BM_KEEP_PACKED|BM_LOAD_PALETTE);

  obj_active = TrackLookup(tracks, "flat3d.object");
  obj_rot_x = TrackLookup(tracks, "flat3d.rotate.x");
  obj_rot_y = TrackLookup(tracks, "flat3d.rotate.y");
  obj_rot_z = TrackLookup(tracks, "flat3d.rotate.z");
  obj_pos_x = TrackLookup(tracks, "flat3d.x");
  obj_pos_y = TrackLookup(tracks, "flat3d.y");
  obj_scale = TrackLookup(tracks, "flat3d.scale");
  flash = TrackLookup(tracks, "flat3d.flash");
  obj_palette = TrackLookup(tracks, "flat3d.palette");
}

static void Prepare() {
  BitmapUnpack(bamboo, BM_DISPLAYABLE);
}

static __interrupt LONG RunEachFrame() {
  UpdateFrameCount();

  if (frameFromStart < 16) {
    FadeBlack(activePal, pal, frameFromStart);
    FadeBlack(bamboo->palette, pal + 16, frameFromStart);
  } else if (frameTillEnd < 16) {
    FadeBlack(activePal, pal, frameTillEnd);
    FadeBlack(bamboo->palette, pal + 16, frameTillEnd);
  } else {
    WORD s = TrackValueGet(flash, frameCount);
    if (s > 0) {
      FadeWhite(activePal, pal, s / 2);
      FadeWhite(bamboo->palette, pal + 16, s / 2);
    }
  }

  return 0;
}

INTERRUPT(FrameInterrupt, 0, RunEachFrame, NULL);

static void UnLoad() {
  DeletePalette(bamboo->palette);
  DeleteBitmap(bamboo);
  DeletePalette(palette[1]);
  DeletePalette(palette[0]);
  DeleteMesh3D(mesh[0]);
  DeleteMesh3D(mesh[1]);
}

static void MakeCopperList(CopListT *cp) {
  CopInsT *sprptr[8];

  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(32), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, bplptr, window0, DEPTH);
  CopSetupSprites(cp, sprptr);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopInsSet32(sprptr[0], sprite[0]->data);
  CopInsSet32(sprptr[1], sprite[1]->data);

  UpdateSprite(sprite[0], X(32), Y(0));
  UpdateSprite(sprite[1], X(256 + 16), Y(0));
}

static void Init() {
  static BitmapT recycled0, recycled1;

  window0 = &recycled0;
  window1 = &recycled1;

  InitSharedBitmap(window0, WIDTH, HEIGHT, DEPTH, screen0);
  InitSharedBitmap(window1, WIDTH, HEIGHT, DEPTH, screen1);

  buffer = NewBitmap(WIDTH, HEIGHT, 1);

  object[0] = NewObject3D(mesh[0]);
  object[0]->translate.z = fx4i(-250);

  object[1] = NewObject3D(mesh[1]);
  object[1]->translate.z = fx4i(-250);

  sprite[0] = NewSpriteFromBitmap(bamboo->height, bamboo, 0, 0);
  sprite[1] = NewSpriteFromBitmap(bamboo->height, bamboo, 0, 0);

  cp = NewCopList(80);
  MakeCopperList(cp);
  CopListActivate(cp);
  EnableDMA(DMAF_BLITTER | DMAF_RASTER | DMAF_SPRITE | DMAF_BLITHOG);

  AddIntServer(INTB_VERTB, FrameInterrupt);
}

static void Kill() {
  RemIntServer(INTB_VERTB, FrameInterrupt);

  DisableDMA(DMAF_COPPER | DMAF_BLITTER | DMAF_RASTER | DMAF_SPRITE |
             DMAF_BLITHOG);

  DeleteCopList(cp);
  DeleteSprite(sprite[0]);
  DeleteSprite(sprite[1]);
  DeleteObject3D(object[0]);
  DeleteObject3D(object[1]);
  DeleteBitmap(buffer);
}

#define MULVERTEX1(D, E) {               \
  WORD t0 = (*v++) + y;                  \
  WORD t1 = (*v++) + x;                  \
  LONG t2 = (*v++) * z;                  \
  v++;                                   \
  D = ((t0 * t1 + t2 - x * y) >> 4) + E; \
}

#define MULVERTEX2(D) {                  \
  WORD t0 = (*v++) + y;                  \
  WORD t1 = (*v++) + x;                  \
  LONG t2 = (*v++) * z;                  \
  WORD t3 = (*v++);                      \
  D = normfx(t0 * t1 + t2 - x * y) + t3; \
}

static __regargs void TransformVertices(Object3D *object) {
  Matrix3D *M = &object->objectToWorld;
  WORD *v = (WORD *)M;
  WORD *src = (WORD *)object->mesh->vertex;
  WORD *dst = (WORD *)object->vertex;
  BYTE *flags = object->vertexFlags;
  register WORD n asm("d7") = object->mesh->vertices - 1;

  LONG m0 = (M->x << 8) - ((M->m00 * M->m01) >> 4);
  LONG m1 = (M->y << 8) - ((M->m10 * M->m11) >> 4);

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

  do {
    if (*flags++) {
      WORD x = *src++;
      WORD y = *src++;
      WORD z = *src++;
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
    } else {
      src += 4;
      dst += 4;
    }
  } while (--n != -1);
}

static void DrawObject(Object3D *object, volatile struct Custom* const custom asm("a6")) {
  IndexListT **faces = object->mesh->face;
  SortItemT *item = object->visibleFace;
  BYTE *faceFlags = object->faceFlags;
  WORD n = object->visibleFaces;
  APTR point = object->vertex;
  APTR temp = buffer->planes[0];

  custom->bltafwm = -1;
  custom->bltalwm = -1;

  for (; --n >= 0; item++) {
    WORD index = item->index;
    IndexListT *face = faces[index];

    WORD minX, minY, maxX, maxY;

    /* Draw edges and calculate bounding box. */
    {
      WORD *i = face->indices;
      register WORD m asm("d7") = face->count - 1;
      WORD *ptr = (WORD *)(point + (WORD)(i[m] << 3));
      WORD xs = *ptr++;
      WORD ys = *ptr++;
      WORD xe, ye;

      minX = xs;
      minY = ys;
      maxX = xs;
      maxY = ys;

      do {
        ptr = (WORD *)(point + (WORD)(*i++ << 3));
        xe = *ptr++;
        ye = *ptr++;

        /* Estimate the size of rectangle that contains a face. */
        if (xe < minX)
          minX = xe;
        else if (xe > maxX)
          maxX = xe;
        if (ye < minY)
          minY = ye;
        else if (ye > maxY)
          maxY = ye;

        /* Draw an edge. */
        {
          WORD x0, y0, dx, dy, derr;
          UWORD bltcon1;

          if (ys < ye) {
            x0 = xs; y0 = ys;
            dx = xe - xs;
            dy = ye - ys;
          } else {
            x0 = xe; y0 = ye;
            dx = xs - xe;
            dy = ys - ye;
          }

          if (dx < 0) {
            dx = -dx;
            if (dx >= dy) {
              bltcon1 = AUL | SUD | LINEMODE | ONEDOT;
            } else {
              bltcon1 = SUL | LINEMODE | ONEDOT;
              swapr(dx, dy);
            }
          } else {
            if (dx >= dy) {
              bltcon1 = SUD | LINEMODE | ONEDOT;
            } else {
              bltcon1 = LINEMODE | ONEDOT;
              swapr(dx, dy);
            }
          }

          derr = dy + dy - dx;
          if (derr < 0)
            bltcon1 |= SIGNFLAG;

          {
            WORD start = ((y0 << 5) + (x0 >> 3)) & ~1;
            APTR dst = temp + start;
            UWORD bltcon0 = rorw(x0 & 15, 4) | BC0F_LINE_EOR;
            UWORD bltamod = derr - dx;
            UWORD bltbmod = dy + dy;
            UWORD bltsize = (dx << 6) + 66;

            WaitBlitter();

            MoveLong(bltbdat, 0xffff, 0x8000);

            custom->bltcon0 = bltcon0;
            custom->bltcon1 = bltcon1;
            custom->bltcpt = dst;
            custom->bltapt = (APTR)(LONG)derr;
            custom->bltdpt = temp;
            custom->bltcmod = WIDTH / 8;
            custom->bltbmod = bltbmod;
            custom->bltamod = bltamod;
            custom->bltdmod = WIDTH / 8;
            custom->bltsize = bltsize;
          }
        }

        xs = xe; ys = ye;
      } while (--m != -1);
    }

    {
      WORD bltstart, bltend;
      UWORD bltmod, bltsize;

      /* Align to word boundary. */
      minX = (minX & ~15) >> 3;
      /* to avoid case where a line is on right edge */
      maxX = ((maxX + 16) & ~15) >> 3;

      {
        WORD w = maxX - minX;
        WORD h = maxY - minY + 1;

        bltstart = minX + minY * (WIDTH / 8);
        bltend = maxX + maxY * (WIDTH / 8) - 2;
        bltsize = (h << 6) | (w >> 1);
        bltmod = (WIDTH / 8) - w;
      }

      /* Fill face. */
      {
        APTR src = temp + bltend;

        WaitBlitter();

        custom->bltcon0 = (SRCA | DEST) | A_TO_D;
        custom->bltcon1 = BLITREVERSE | FILL_XOR;
        custom->bltapt = src;
        custom->bltdpt = src;
        custom->bltamod = bltmod;
        custom->bltbmod = bltmod;
        custom->bltdmod = bltmod;
        custom->bltsize = bltsize;
      }

      /* Copy filled face to screen. */
      {
        APTR *screen = &window0->planes[DEPTH];
        APTR src = temp + bltstart;
        BYTE mask = 1 << (DEPTH - 1);
        BYTE color = faceFlags[index];
        WORD n = DEPTH;

        while (--n >= 0) {
          APTR dst = *(--screen) + bltstart;
          UWORD bltcon0;

          if (color & mask)
            bltcon0 = (SRCA | SRCB | DEST) | A_OR_B;
           else
            bltcon0 = (SRCA | SRCB | DEST) | (NABC | NABNC);

          WaitBlitter();

          custom->bltcon0 = bltcon0;
          custom->bltcon1 = 0;
          custom->bltapt = src;
          custom->bltbpt = dst;
          custom->bltdpt = dst;
          custom->bltsize = bltsize;

          mask >>= 1;
        }
      }

      /* Clear working area. */
      {
        APTR data = temp + bltstart;

        WaitBlitter();

        custom->bltcon0 = (DEST | A_TO_D);
        custom->bltadat = 0;
        custom->bltdpt = data;
        custom->bltsize = bltsize;
      }
    }
  }
}

static void Render() {
  Object3D *cube = object[TrackValueGet(obj_active, frameCount)];
  BitmapClear(window0);

  cube->rotate.x = TrackValueGet(obj_rot_x, frameCount) * 4;
  cube->rotate.y = TrackValueGet(obj_rot_y, frameCount) * 4;
  cube->rotate.z = TrackValueGet(obj_rot_z, frameCount) * 4;
  cube->scale.x = cube->scale.y = cube->scale.z =
    TrackValueGet(obj_scale, frameCount) * 4;
  cube->translate.x = TrackValueGet(obj_pos_x, frameCount);
  cube->translate.y = TrackValueGet(obj_pos_y, frameCount);

  UpdateObjectTransformation(cube);
  UpdateFaceVisibility(cube);
  UpdateVertexVisibility(cube);
  TransformVertices(cube);
  SortFaces(cube);
  DrawObject(cube, custom);

  {
    WORD palnum = TrackValueGet(obj_palette, frameCount);
    if (palnum)
      activePal = palette[palnum - 1];
  }

  CopUpdateBitplanes(bplptr, window0, DEPTH);
  TaskWait(VBlankEvent);
  swapr(window0, window1);
}

EFFECT(FlatShade, Load, UnLoad, Init, Kill, Render, Prepare);
