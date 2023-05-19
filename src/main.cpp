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
		_ProcessMessageCrf = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_CraftingMenu[0])).write_vfunc(0x4, ProcessMessageCrf);
		_ProcessMessageCon = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_ContainerMenu[0])).write_vfunc(0x4, ProcessMessageCon);
		_ProcessMessageBar = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_BarterMenu[0])).write_vfunc(0x4, ProcessMessageBar);
	}

private:
	static bool is_shield(RE::InventoryMenu* menu)
	{
		if (auto selected = _generic_foo_<50086, RE::ItemList::Item*(RE::ItemList*)>::eval(menu->itemList);
			selected && selected->data.objDesc && selected->data.objDesc->GetObject()) {
			if (auto shield_ = selected->data.objDesc->GetObject(); shield_ && shield_->As<RE::TESObjectARMO>()) {
				if ((uint32_t)shield_->As<RE::TESObjectARMO>()->GetSlotMask() &
					(uint32_t)RE::BIPED_MODEL::BipedObjectSlot::kShield) {
					return true;
				}
			}
		}

		return false;
	}

	static void update_label(RE::InventoryMenu* menu)
	{
		if (auto movie = menu->uiMovie.get(); menu->itemList && movie) {
			RE::GFxValue ApparelArmorLabel;
			if (movie->GetVariable(&ApparelArmorLabel, "_root.Menu_mc.itemCardFadeHolder.ItemCard_mc.ApparelArmorLabel")) {
				if (is_shield(menu)) {
					ApparelArmorLabel.SetMember("htmlText", "$f314_STB_TestUI_SHIELD_LABEL");
				} else {
					ApparelArmorLabel.SetMember("htmlText", "$ARMOR");
				}
			}
		}
	}

	static RE::UI_MESSAGE_RESULTS ProcessMessageInv(RE::InventoryMenu* menu, RE::UIMessage& a_message)
	{
		update_label(menu);
		return _ProcessMessageInv(menu, a_message);
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageGft(RE::InventoryMenu* menu, RE::UIMessage& a_message)
	{
		update_label(menu);
		return _ProcessMessageGft(menu, a_message);
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageCrf(RE::InventoryMenu* menu, RE::UIMessage& a_message)
	{
		update_label(menu);
		return _ProcessMessageCrf(menu, a_message);
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageCon(RE::InventoryMenu* menu, RE::UIMessage& a_message)
	{
		update_label(menu);
		return _ProcessMessageCon(menu, a_message);
	}
	static RE::UI_MESSAGE_RESULTS ProcessMessageBar(RE::InventoryMenu* menu, RE::UIMessage& a_message)
	{
		update_label(menu);
		return _ProcessMessageBar(menu, a_message);
	}

	static inline REL::Relocation<decltype(ProcessMessageInv)> _ProcessMessageInv;
	static inline REL::Relocation<decltype(ProcessMessageInv)> _ProcessMessageGft;
	static inline REL::Relocation<decltype(ProcessMessageInv)> _ProcessMessageCrf;
	static inline REL::Relocation<decltype(ProcessMessageInv)> _ProcessMessageCon;
	static inline REL::Relocation<decltype(ProcessMessageInv)> _ProcessMessageBar;
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
