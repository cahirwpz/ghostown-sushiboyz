SOURCES_C = ninja.c \
            toilet.c \
            floor.c \
            anim.c \
            blurred3d.c \
            logo.c \
            bumpmap-rgb.c \
            twister-rgb.c \
            watchmaker.c \
            uvmap.c \
            filled3d.c \
            credits.c \
            thunders.c \
            flatshade.c \
            sushigirl.c \
            glitch.c

DATA := nigiri/01_ninja_anim1.iff \
        nigiri/01_ninja_anim2.iff \
        nigiri/01_ninja_anim3.iff \
        nigiri/01_ninja_anim4.iff \
        nigiri/01_ninja_bkg_sold.iff \
        nigiri/01_ninja_standing.iff \
        nigiri/02_ninja_leb_maska.iff \
        nigiri/02_ninja_oczy_male.iff \
        nigiri/03_oko_lewe_lewo.iff \
        nigiri/03_oko_lewe_lewo_s.iff \
        nigiri/03_oko_lewe_prawo.iff \
        nigiri/03_oko_lewe_prawo_s.iff \
        nigiri/03_oko_prawe_lewo.iff \
        nigiri/03_oko_prawe_lewo_s.iff \
        nigiri/03_oko_prawe_prawo.iff \
        nigiri/03_oko_prawe_prawo_s.iff \
        nigiri/04_ninja_big_maska.iff \
        nigiri/04_ninja_oczy_duze.iff \
        nigiri/04_ninja_wkurw.iff \
        nigiri/05_kiblo_kulka.iff \
        nigiri/05_ninja_kibel.iff \
        nigiri/06_ball.iff \
        nigiri/07_floor.iff \
        nigiri/07_floor_stripes.iff \
        nigiri/08_bamboo.iff \
        nigiri/21_ada.iff \
        nigiri/21_dek.iff \
        nigiri/21_des.iff \
        nigiri/21_dre.iff \
        nigiri/21_elu.iff \
        nigiri/21_foc.iff \
        nigiri/21_hau.iff \
        nigiri/21_lam.iff \
        nigiri/21_lon.iff \
        nigiri/21_moo.iff \
        nigiri/21_nah.iff \
        nigiri/21_pal1.iff \
        nigiri/21_pal2.iff \
        nigiri/21_pal3.iff \
        nigiri/21_rno.iff \
        nigiri/21_ska.iff \
        nigiri/21_spa.iff \
        nigiri/21_spe.iff \
        nigiri/21_wan.iff \
        nigiri/22_sushi_girl.iff \
        nigiri/23_the_end_logo.iff \
        nigiri/p61.jazzcat-sushiboyz \
        nigiri/smp.jazzcat-sushiboyz \
        nigiri/sushiboyz.sync

LIBS := lib3d libsync libp61

all: sushiboyz.exe 

include build.mk

archive:
	7z a "sushiboyz-$$(date +%F-%H%M).7z" sushiboyz nigiri

# vim: ts=8 sw=8 expandtab
