# party files are run through trainerproc, which is a tool that converts party data to an output file
# matching the current trainer .h formatting

AUTO_GEN_TARGETS += src/data/trainers.h
AUTO_GEN_TARGETS += src/data/trainers_frlg.h
AUTO_GEN_TARGETS += src/data/battle_partners.h
AUTO_GEN_TARGETS += test/battle/trainer_control.h
AUTO_GEN_TARGETS += test/battle/partner_control.h
AUTO_GEN_TARGETS += src/data/debug_trainers.h

%.h: %.party $(TRAINERPROC)
	$(CPP) $(CPPFLAGS) -traditional-cpp - < $< | $(TRAINERPROC) -o $@ -i $< -

# Harness di self-play VGC (test/vgc_selfplay): gli allenatori VGC (linea AI con "Vgc Doubles") estratti da
# src/data/trainers.party, così il ROM dei test li ha con i set veri e resta sempre allineato.
AUTO_GEN_TARGETS += test/vgc_selfplay/vgc_trainers.party
AUTO_GEN_TARGETS += test/vgc_selfplay/vgc_trainers.h

test/vgc_selfplay/vgc_trainers.party: src/data/trainers.party
	@mkdir -p $(dir $@)
	awk '/^=== /{ if (keep) printf "%s", buf; buf = ""; keep = 0 } { buf = buf $$0 "\n" } /^AI:.*Vgc Doubles/{ keep = 1 } END { if (keep) printf "%s", buf }' $< > $@
