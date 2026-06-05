#ifndef D_ALBW_OOCOO_H
#define D_ALBW_OOCOO_H

// ============================================
// NEW CODE — ALBW Port
// Postman "Cuckoo's Return" — paid dungeon-entrance warp after a stripped
// dungeon death when the player chose to respawn in Ordon Village.
// Not an inventory item; offered as a shop row at the rental Postman.
// ============================================
#if TARGET_PC

// Call from game-over flow when the player confirms Continue.
void dALBWOocoo_onDeathWarpContext(const char* i_lastStageName, bool i_diedInDungeon);
void dALBWOocoo_onWarpChoice(int i_choice);  // 0 = entrance/continue, 1 = Ordon

bool dALBWOocoo_canShowInShop();
bool dALBWOocoo_tryPurchase();  // deducts rupees; schedules warp after shop closes

// Called once when the Postman talk event ends after a successful purchase.
void dALBWOocoo_executePendingWarp();

const char* dALBWOocoo_getServiceName();
const char* dALBWOocoo_getServiceDesc();
int         dALBWOocoo_getServicePrice();

#endif  // TARGET_PC

#endif /* D_ALBW_OOCOO_H */
