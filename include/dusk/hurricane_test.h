#pragma once



#include "types.h"



class daAlink_c;



namespace dusk {



#if TARGET_PC

bool isHurricaneTestEnabled();

bool isLinkHurricaneProc(const daAlink_c* i_link);

bool isHurricaneWhirlwindVfx();
bool isHurricaneBaseVfx();

u32 getHurricanePlaceholderVoiceSe();

int getHurricaneTestSeIndex();

int getHurricaneTestSeCount();

const char* getHurricaneTestSeName(int index);

void initHurricaneSpinSe(daAlink_c* link);

void maintainHurricaneSpinSe(daAlink_c* link);

void stopHurricaneSpinSe(daAlink_c* link);

#else

inline bool isHurricaneTestEnabled() {

    return false;

}

inline bool isLinkHurricaneProc(const daAlink_c*) {

    return false;

}

#endif



}  // namespace dusk

