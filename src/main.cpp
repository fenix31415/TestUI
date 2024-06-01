extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

class ItemCardHook
{
public:
	static void Hook()
	{
		_ProcessMessageInv = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_InventoryMenu[0])).write_vfunc(0x4, ProcessMessageInv);
		_ProcessMessageGft = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_GiftMenu[0])).write_vfunc(0x4, ProcessMessageGft);
		_ProcessMessageCon = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_ContainerMenu[0])).write_vfunc(0x4, ProcessMessageCon);
		_ProcessMessageBar = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_BarterMenu[0])).write_vfunc(0x4, ProcessMessageBar);

		auto& trmpl = SKSE::GetTrampoline();
		_SetItemCardInfoCrf = trmpl.write_call<5>(REL::ID(50566).address() + 0x6a, SetItemCardInfoCrf);    // SkyrimSE.exe+8743BA
		_SetItemCardInfoEnc = trmpl.write_branch<5>(REL::ID(50561).address() + 0xa, SetItemCardInfoEnc);   // SkyrimSE.exe+873A9A
		_SetItemCardInfoSm1 = trmpl.write_call<5>(REL::ID(50568).address() + 0x30, SetItemCardInfoSm1);    // SkyrimSE.exe+874840
		_SetItemCardInfoSm2 = trmpl.write_call<5>(REL::ID(50568).address() + 0x7b, SetItemCardInfoSm2);    // SkyrimSE.exe+87488B
	}

private:
	static bool is_shield(RE::ItemList* itemlist)
	{
		if (itemlist && !itemlist->root.IsNull()) {
			if (auto selected = _generic_foo_<50086, RE::ItemList::Item*(RE::ItemList*)>::eval(itemlist);
				selected && selected->data.objDesc) {
				return is_shield(selected->data.objDesc);
			}
		}

		return false;
	}

	template <typename Menu>
	static void update_label(Menu* menu)
	{
		static_assert(std::is_base_of<RE::IMenu, Menu>::value);

		if (auto movie = menu->uiMovie.get(); menu->itemList && movie) {
			RE::GFxValue ApparelArmorLabel;
			if (movie->GetVariable(&ApparelArmorLabel, "_root.Menu_mc.itemCardFadeHolder.ItemCard_mc.ApparelArmorLabel")) {
				update_label(ApparelArmorLabel, is_shield(menu->itemList));
			}
		}
	}

	static RE::UI_MESSAGE_RESULTS ProcessMessageInv(RE::InventoryMenu* menu, RE::UIMessage& a_message)
	{
		auto ans = _ProcessMessageInv(menu, a_message);
		if (a_message.type.get() != RE::UI_MESSAGE_TYPE::kHide)
			update_label(menu);
		return ans;
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageGft(RE::GiftMenu* menu, RE::UIMessage& a_message)
	{
		auto ans = _ProcessMessageGft(menu, a_message);
		if (a_message.type.get() != RE::UI_MESSAGE_TYPE::kHide)
			update_label(menu);
		return ans;
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageCon(RE::ContainerMenu* menu, RE::UIMessage& a_message)
	{
		auto ans = _ProcessMessageCon(menu, a_message);
		if (a_message.type.get() != RE::UI_MESSAGE_TYPE::kHide)
			update_label(menu);
		return ans;
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageBar(RE::BarterMenu* menu, RE::UIMessage& a_message)
	{
		auto ans = _ProcessMessageBar(menu, a_message);
		if (a_message.type.get() != RE::UI_MESSAGE_TYPE::kHide)
			update_label(menu);
		return ans;
	}

	static inline REL::Relocation<decltype(ProcessMessageInv)> _ProcessMessageInv;
	static inline REL::Relocation<decltype(ProcessMessageGft)> _ProcessMessageGft;
	static inline REL::Relocation<decltype(ProcessMessageCon)> _ProcessMessageCon;
	static inline REL::Relocation<decltype(ProcessMessageBar)> _ProcessMessageBar;

	static bool is_shield(RE::InventoryEntryData* item)
	{
		if (auto shield_ = item->GetObject(); shield_ && shield_->As<RE::TESObjectARMO>()) {
			if ((uint32_t)shield_->As<RE::TESObjectARMO>()->GetSlotMask() & (uint32_t)RE::BIPED_MODEL::BipedObjectSlot::kShield) {
				return true;
			}
		}

		return false;
	}

	static void update_label(RE::GFxValue& ApparelArmorLabel, bool to_new)
	{
		if (to_new) {
			ApparelArmorLabel.SetMember("htmlText", "$f314_STB_TestUI_SHIELD_LABEL");
		} else {
			ApparelArmorLabel.SetMember("htmlText", "$ARMOR");
		}
	}

	static void update_label(RE::CraftingSubMenus::CraftingSubMenu* menu, RE::InventoryEntryData* item)
	{
		if (auto movie = menu->view) {
			RE::GFxValue ApparelArmorLabel;
			if (movie->GetVariable(&ApparelArmorLabel, "_root.Menu.ItemInfo.ApparelArmorLabel")) {
				update_label(ApparelArmorLabel, is_shield(item));
			}
		}
	}

	static void SetItemCardInfoCrf(RE::CraftingSubMenus::CraftingSubMenu* menu, RE::InventoryEntryData* item)
	{
		_SetItemCardInfoCrf(menu, item);
		update_label(menu, item);
	}
	static void SetItemCardInfoEnc(RE::CraftingSubMenus::CraftingSubMenu* menu, RE::InventoryEntryData* item)
	{
		_SetItemCardInfoCrf(menu, item);
		update_label(menu, item);
	}
	static void SetItemCardInfoSm1(RE::CraftingSubMenus::CraftingSubMenu* menu, RE::InventoryEntryData* item)
	{
		_SetItemCardInfoSm1(menu, item);
		update_label(menu, item);
	}
	static void SetItemCardInfoSm2(RE::CraftingSubMenus::CraftingSubMenu* menu, RE::InventoryEntryData* item)
	{
		_SetItemCardInfoSm2(menu, item);
		update_label(menu, item);
	}

	static inline REL::Relocation<decltype(SetItemCardInfoCrf)> _SetItemCardInfoCrf;
	static inline REL::Relocation<decltype(SetItemCardInfoEnc)> _SetItemCardInfoEnc;
	static inline REL::Relocation<decltype(SetItemCardInfoSm1)> _SetItemCardInfoSm1;
	static inline REL::Relocation<decltype(SetItemCardInfoSm2)> _SetItemCardInfoSm2;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		ItemCardHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
