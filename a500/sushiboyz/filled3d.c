#include "sushiboyz.h"
#include "blitter.h"
#include "coplist.h"
#include "3d.h"
#include "fx.h"
#include "ffp.h"
#include "memory.h"
#include "ilbm.h"

#define WIDTH  256
#define HEIGHT 256
#define DEPTH  3

#define GREETZ 16

static Mesh3D *mesh;
static Object3D *cube;
static CopListT *cp;
static CopInsT *bplptr[DEPTH];
static BitmapT *window0, *window1;
static BitmapT *buffer;
static CopInsT *pal;
static BitmapT *greetz[GREETZ];
static UWORD palette[4][8];
static WORD activePal = 0;

static TrackT *pistons_view, *pistons_palette, *greetz_num;

static void Load() {
  mesh = LoadMesh3D("21_pistons.3d", SPFlt(28));
  CalculateFaceNormals(mesh);

  greetz[0] = LoadILBMCustom("21_ada.iff", BM_KEEP_PACKED);
  greetz[1] = LoadILBMCustom("21_dek.iff", BM_KEEP_PACKED);
  greetz[2] = LoadILBMCustom("21_des.iff", BM_KEEP_PACKED);
  greetz[3] = LoadILBMCustom("21_dre.iff", BM_KEEP_PACKED);
  greetz[4] = LoadILBMCustom("21_elu.iff", BM_KEEP_PACKED);
  greetz[5] = LoadILBMCustom("21_foc.iff", BM_KEEP_PACKED);
  greetz[6] = LoadILBMCustom("21_hau.iff", BM_KEEP_PACKED);
  greetz[7] = LoadILBMCustom("21_lam.iff", BM_KEEP_PACKED);
  greetz[8] = LoadILBMCustom("21_lon.iff", BM_KEEP_PACKED);
  greetz[9] = LoadILBMCustom("21_moo.iff", BM_KEEP_PACKED);
  greetz[10] = LoadILBMCustom("21_nah.iff", BM_KEEP_PACKED);
  greetz[11] = LoadILBMCustom("21_rno.iff", BM_KEEP_PACKED);
  greetz[12] = LoadILBMCustom("21_ska.iff", BM_KEEP_PACKED);
  greetz[13] = LoadILBMCustom("21_spa.iff", BM_KEEP_PACKED);
  greetz[14] = LoadILBMCustom("21_spe.iff", BM_KEEP_PACKED);
  greetz[15] = LoadILBMCustom("21_wan.iff", BM_KEEP_PACKED);

  palette[0][0] = 0x012;
  palette[0][1] = 0x5AF;
  palette[0][2] = 0x28C;
  palette[0][3] = 0x06A;
  palette[0][4] = 0x678;
  palette[0][5] = 0xBFF;
  palette[0][6] = 0x8EF;
  palette[0][7] = 0x6CF;

  {
    PaletteT *pal = LoadPalette("21_pal1.iff");
    ConvertPaletteToRGB4(pal, palette[1], 8);
    DeletePalette(pal);
  }

  {
    PaletteT *pal = LoadPalette("21_pal2.iff");
    ConvertPaletteToRGB4(pal, palette[2], 8);
    DeletePalette(pal);
  }

  {
    PaletteT *pal = LoadPalette("21_pal3.iff");
    ConvertPaletteToRGB4(pal, palette[3], 8);
    DeletePalette(pal);
  }

  mesh->origVertex = MemAlloc(sizeof(Point3D) * mesh->vertices, MEMF_PUBLIC);
  memcpy(mesh->origVertex, mesh->vertex, sizeof(Point3D) * mesh->vertices);

  greetz_num = TrackLookup(tracks, "pistons.greetz");
  pistons_view = TrackLookup(tracks, "pistons.view");
  pistons_palette = TrackLookup(tracks, "pistons.palette");
}

static void Prepare() {
  WORD n = GREETZ;

  while (--n >= 0)
    BitmapUnpack(greetz[n], BM_DISPLAYABLE);
}

static void UnLoad() {
  WORD n = GREETZ;

  while (--n >= 0)
    DeleteBitmap(greetz[n]);

  DeleteMesh3D(mesh);
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
  LONG m2 = M->z;

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

  M->z = m2;
}

static void DrawObject(Object3D *object, volatile struct Custom* const custom asm("a6")) {
  IndexListT **faces = object->mesh->face;
  SortItemT *item = object->visibleFace;
  BYTE *faceSurface = object->mesh->faceSurface;
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
              bltcon1 = AUL | SUD | LINE_ONEDOT;
            } else {
              bltcon1 = SUL | LINE_ONEDOT;
              swapr(dx, dy);
            }
          } else {
            if (dx >= dy) {
              bltcon1 = SUD | LINE_ONEDOT;
            } else {
              bltcon1 = LINE_ONEDOT;
              swapr(dx, dy);
            }
          }

          /*
           * We can safely (?) assume that:
           * -WIDTH / 2 < dx < WIDTH / 2
           * -HEIGHT / 2 < dy < HEIGTH / 2
           */

          derr = dy + dy - dx;
          if (derr < 0)
            bltcon1 |= SIGNFLAG;

          {
            WORD start = ((y0 << 5) + (x0 >> 3)) & ~1;
            APTR dst = temp + start;
            UWORD bltcon0 = rorw(x0 & 15, 4) | LINE_EOR;
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
        APTR *screen = &window0->planes[DEPTH - 1];
        APTR src = temp + bltstart;
        BYTE mask = 1 << (DEPTH - 2);
        BYTE color = faceSurface[index];
        WORD n = DEPTH - 1;

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

        custom->bltcon0 = DEST;
        custom->bltadat = 0;
        custom->bltdpt = data;
        custom->bltsize = bltsize;
      }
    }
  }
}

#define VIEWS 5

WORD views[5][6] = {
  { fx4i(20), fx4i(20), fx4i(-250), 0x100, -0x100, 0 },
  { fx4i(-20), fx4i(20), fx4i(-250), 0x100, 0x100, 0 },
  { fx4i(-20), fx4i(-20), fx4i(-250), -0x100, 0x100, 0 },
  { fx4i(20), fx4i(-20), fx4i(-250), -0x100, -0x100, 0 },
  { fx4i(0), fx4i(0), fx4i(-250), 0, 0, 0 }
};

static void SwitchView(WORD num) {
  WORD *view;

  if (num >= VIEWS)
    num = VIEWS - 1;
  if (view < 0)
    num = 0;

  view = views[num];

  cube->translate.x = *view++;
  cube->translate.y = *view++;
  cube->translate.z = *view++;
  cube->rotate.x = *view++;
  cube->rotate.y = *view++;
  cube->rotate.z = *view++;

  {
    Point3D *temp = cube->mesh->vertex;

    cube->mesh->vertex = cube->mesh->origVertex;

    UpdateObjectTransformation(cube);
    UpdateFaceVisibility(cube);
    UpdateVertexVisibility(cube);
    TransformVertices(cube);
    SortFaces(cube);

    cube->mesh->vertex = temp;
  }

  memset(cube->vertexFlags, 0, cube->mesh->vertices);
}

static void MakeCopperList(CopListT *cp) {
  WORD i;

  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(32), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, bplptr, window0, DEPTH);
  pal = CopLoadColor(cp, 0, 7, 0);
  CopEnd(cp);
 
  for (i = 0; i < 8; i++)
    CopInsSet16(pal + i, palette[0][i]);
}

static void Init() {
  static BitmapT recycled0, recycled1;

  window0 = &recycled0;
  window1 = &recycled1;

  InitSharedBitmap(window0, WIDTH, HEIGHT, DEPTH, screen0);
  InitSharedBitmap(window1, WIDTH, HEIGHT, DEPTH, screen1);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_BLITHOG;

  BitmapClear(window0);
  BitmapClear(window1);

  buffer = NewBitmap(WIDTH, HEIGHT, 1);

  cube = NewObject3D(mesh);
  cube->translate.x = fx4i(20);
  cube->translate.y = fx4i(20);
  cube->translate.z = fx4i(-250);

  cp = NewCopList(80);
  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
  
  SwitchView(0);
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_BLITHOG;

  DeleteBitmap(buffer);
  DeleteCopList(cp);
  DeleteObject3D(cube);
}

static void ModifyGeometry(Object3D *object) {
  IndexListT **faces = object->mesh->face;
  BYTE *faceSurface = object->mesh->faceSurface;
  WORD n = object->mesh->faces;
  Point3D *vertex = object->mesh->vertex;
  BYTE *vertexFlags = object->vertexFlags; 
  WORD f = frameCount * 64;

  memset(vertexFlags, 0, object->mesh->vertices);

  while (--n >= 0) {
    UBYTE surface = *faceSurface++;

    if (surface == 1) {
      IndexListT *face = *faces++;
      WORD count = face->count;
      WORD *index = face->indices;
      WORD z = fx4i(192 / 4) + SIN(f + n * 32) / 8;

      while (--count >= 0) {
        WORD i = *index++;
        *(WORD *)((APTR)vertex + (WORD)(i << 3) + 4) = z;
        vertexFlags[i] = -1;
      }
    } else {
      faces++;
    }
  }
}

static void Render() {
  {
    WORD event = TrackValueGet(pistons_palette, frameCount);

    if (event > 0) {
      UWORD *colors;
      WORD i;

      activePal = event - 1;
     
      colors = palette[activePal];

      for (i = 0; i < 8; i++)
        CopInsSet16(pal + i, *colors++);
    }
  }

  {
    WORD event = TrackValueGet(pistons_view, frameCount);

    if (event > 0) {
      UWORD *colors = palette[activePal];
      WORD i;

      for (i = 0; i < 8; i++)
        CopInsSet16(pal + i, 0xFFF);

      SwitchView(event - 1);

      for (i = 0; i < 8; i++)
        CopInsSet16(pal + i, *colors++);
    }
  }
  
  {
    WORD event = TrackValueGet(greetz_num, frameCount);
    if (event > 0) {
      BitmapT *greet = greetz[event - 1];

      BlitterClear(window0, 2);
      BlitterClear(window1, 2);
      BlitterCopy(window0, 2, (WIDTH - greet->width) / 2, (HEIGHT - greet->height) / 2, greet, 0);
      BlitterCopy(window1, 2, (WIDTH - greet->width) / 2, (HEIGHT - greet->height) / 2, greet, 0);
    }
  }

  // PROFILE_BEGIN(filled3d);

  {
    UWORD bltsize = (WIDTH << 6) | ((HEIGHT * 2) >> 4);
    APTR bltpt = window0->planes[0];

    WaitBlitter();

    custom->bltadat = 0;
    custom->bltdmod = 0;
    custom->bltcon0 = DEST;
    custom->bltcon1 = 0;
    custom->bltdpt = bltpt;
    custom->bltsize = bltsize;
  }

  ModifyGeometry(cube);
  TransformVertices(cube);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
  DrawObject(cube, custom);
  custom->dmacon = DMAF_BLITHOG;
  // PROFILE_END(filled3d);

  WaitVBlank();
  CopUpdateBitplanes(bplptr, window0, DEPTH);
  swapr(window0, window1);
}

EFFECT(Filled3D, Load, UnLoad, Init, Kill, Render, Prepare);
