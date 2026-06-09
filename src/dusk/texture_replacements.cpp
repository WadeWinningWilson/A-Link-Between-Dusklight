#include "dusk/texture_replacements.hpp"

#include <aurora/gfx.h>

#include "dusk/settings.h"

namespace dusk::texture_replacements {

void reload() {
    aurora_set_texture_replacements_enabled(
        getSettings().game.enableTextureReplacements.getValue());
    aurora_reload_texture_replacements();
}

void set_enabled(bool enabled) {
    getSettings().game.enableTextureReplacements.setValue(enabled);
    aurora_set_texture_replacements_enabled(enabled);
}

void shutdown() {
    aurora_set_texture_replacements_enabled(false);
}

}  // namespace dusk::texture_replacements
