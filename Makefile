#
# This file is part of The Lost Vikings Library/Tools
#
# Ryan Mallon, 2016, <rmallon@gmail.com>
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to
# the public domain worldwide. This software is distributed without
# any warranty.  You should have received a copy of the CC0 Public
# Domain Dedication along with this software. If not, see
#
# <http://creativecommons.org/publicdomain/zero/1.0/>.
#

CFLAGS = -g -Wall -I.
LFLAGS = -lSDL

liblv_dir := 		liblv	
liblv_objs :=		liblv/buffer.o		\
			liblv/lv_debug.o	\
			liblv/lv_level.o 	\
			liblv/lv_pack.o		\
			liblv/lv_compress.o	\
			liblv/lv_sprite.o	\
			liblv/lv_object_db.o
liblv_a :=		liblv.a

pack_tool_objs :=	pack_tool.o

level_view_objs :=	level_view.o		\
			sdl_helpers.o

sprite_view_objs :=	sprite_view.o

all_objs :=		$(liblv_objs)		\
			$(pack_tool_objs)	\
			$(level_view_objs)	\
			$(sprite_view_objs)

all_progs :=		pack_tool		\
			level_view		\
			sprite_view

all: $(all_progs)

%.o: %.c
	@echo "  CC $<"
	@$(CC) -c -o $@ $< $(CFLAGS)

$(liblv_a): $(liblv_objs)
	@echo "  AR $@"
	@$(AR) crs $@ $(liblv_objs)

pack_tool: $(liblv_a) $(pack_tool_objs)
	@echo "  LD $@"
	@$(CC) -o $@ $(pack_tool_objs) $(LFLAGS) $(liblv_a)

level_view: $(liblv_a) $(level_view_objs)
	@echo "  LD $@"
	@$(CC) -o $@ $(level_view_objs) $(LFLAGS) $(liblv_a)

sprite_view: $(liblv_a) $(sprite_view_objs)
	@echo "  LD $@"
	@$(CC) -o $@ $(sprite_view_objs) $(LFLAGS) $(liblv_a)

.PHONY: docs
docs: doxygen.dox
	@echo "  DOXYGEN $@"
	@doxygen doxygen.dox

clean:
	@echo "  CLEAN"
	@rm -Rf $(liblv_a) $(all_progs) $(all_objs)