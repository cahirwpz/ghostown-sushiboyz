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
            glitch.c \
            loading.c \
            music.c \
            palops.c \
            worker.c

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
        nigiri/08_codi.3d \
        nigiri/08_flatshade_pal.iff \
        nigiri/08_flatshade_pal2.iff \
        nigiri/08_kolczak1.3d \
        nigiri/08_kolczak2.3d \
        nigiri/08_tree2.3d \
        nigiri/09_ball.3d \
        nigiri/09_running_anim.bin \
        nigiri/09_running_pal.iff \
        nigiri/11_cube.3d \
        nigiri/11_klip.iff \
        nigiri/11_palette.png \
        nigiri/12_ghostown_logo.iff \
        nigiri/13_bumpmap.bin \
        nigiri/13_bumpmap_image.png \
        nigiri/13_bumpmap_light.png \
        nigiri/13_insert_coin.iff \
        nigiri/16_01_cahir.iff \
        nigiri/16_02_slayer.iff \
        nigiri/16_03_jazzcat.iff \
        nigiri/16_04_dkl.iff \
        nigiri/16_05_dance1.iff \
        nigiri/16_06_dance2.iff \
        nigiri/16_07_dance3.iff \
        nigiri/16_08_dance4.iff \
        nigiri/16_ball.bin \
        nigiri/16_ball_texture.png \
        nigiri/16_credits_logo.iff \
        nigiri/16_discoball.iff \
        nigiri/16_floor.iff \
        nigiri/16_insert_coin.iff \
        nigiri/16_txt_cahir.iff \
        nigiri/16_txt_codi.iff \
        nigiri/16_txt_dkl.iff \
        nigiri/16_txt_jazz.iff \
        nigiri/16_txt_slay.iff \
        nigiri/17_thunders.iff \
        nigiri/17_thunders_floor.png \
        nigiri/18_twister.iff \
        nigiri/18_twister_gradient.iff \
        nigiri/18_twister_left.iff \
        nigiri/18_twister_right.iff \
        nigiri/18_twister_texture.png \
        nigiri/19_the_watchmaker_of_light.iff \
        nigiri/20_klip.iff \
        nigiri/20_uvmap.bin \
        nigiri/20_uvmap_texture.png \
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
        nigiri/21_pistons.3d \
        nigiri/22_sushi_girl.iff \
        nigiri/23_the_end_logo.iff \
        nigiri/p61.jazzcat-sushiboyz \
        nigiri/smp.jazzcat-sushiboyz \
        nigiri/sushiboyz.sync \
        nigiri/boot.ilbm

LIBS := lib3d libsync libp61

all: build-nigiri sushiboyz.exe 

include build.mk

archive:
	7z a "sushiboyz-$$(date +%F-%H%M).7z" sushiboyz nigiri

clean:: clean-nigiri

# vim: ts=8 sw=8 expandtab
