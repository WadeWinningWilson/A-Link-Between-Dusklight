#ifndef D_ALBW_COMBAT_H
#define D_ALBW_COMBAT_H

#if TARGET_PC

#include "f_op/f_op_actor.h"

enum dAlbwHelmBashTier {
    dAlbwHelmBash_THRESHOLD = 0,
    dAlbwHelmBash_MAX = 1,
};

// Bash spend required to open a Helm Splitter punish credit on this target.
// Default: threshold (tier bashThreshold for Link's shield). Armored Darknut: max bar.
dAlbwHelmBashTier dAlbwCombat_getHelmBashTier(fopAc_ac_c* i_actor);

bool dAlbw_isHiddenSkillReworkEnabled();

// Jump Strike charge gate (Hidden Skill Rework). Set once charge anim reaches frame 6.
void dAlbw_resetJumpStrikeChargeReady();
void dAlbw_setJumpStrikeChargeReady();
bool dAlbw_isJumpStrikeChargeReady();
bool dAlbw_peekJumpStrikeChargeReady();
bool dAlbw_tryConsumeJumpStrikeChargeReady();

#endif // TARGET_PC

#endif // D_ALBW_COMBAT_H
