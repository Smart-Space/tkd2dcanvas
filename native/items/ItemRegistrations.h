// ItemRegistrations.h — registers the built-in item types with ItemFactory.
#pragma once

namespace tkd2dcanvas {

// Registers "rectangle", "oval" and "line" with ItemFactory::Instance().
// Idempotent; called once per interpreter load from Tkd2dcanvas_Init.
void RegisterBuiltinItems();

}  // namespace tkd2dcanvas
