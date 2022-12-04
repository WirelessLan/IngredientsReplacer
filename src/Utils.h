#pragma once

namespace Utils {

	RE::TESObjectREFR* GetCurrentFurniture(RE::Actor* a_actor) {
		if (!a_actor || !a_actor->currentProcess || !a_actor->currentProcess->middleHigh)
			return nullptr;

		RE::NiPointer<RE::TESObjectREFR> currFurn = a_actor->currentProcess->middleHigh->currentFurniture.get();
		if (currFurn)
			return currFurn.get();

		return nullptr;
	}

	RE::TESObjectREFR* GetParentWorkshop(RE::TESObjectREFR* a_furn) {
		if (!a_furn)
			return nullptr;

		using func_t = decltype(&GetParentWorkshop);
		REL::Relocation<func_t> func{ REL::ID(266741) };

		return func(a_furn);
	}

	uint32_t GetInventoryItemCount(RE::TESObjectREFR* a_refr, RE::TESForm* a_item) {
		if (!a_refr || !a_item)
			return 0;

		RE::BGSInventoryList* inventory = a_refr->inventoryList;
		if (!inventory)
			return 0;

		uint32_t totalItemCount = 0;
		inventory->rwLock.lock_read();
		for (RE::BGSInventoryItem& item : inventory->data) {
			if (item.object == a_item) {
				RE::BGSInventoryItem::Stack* sp = item.stackData.get();
				while (sp) {
					totalItemCount += sp->count;
					sp = sp->nextStack.get();
				}
				break;
			}
		}
		inventory->rwLock.unlock_read();

		return totalItemCount;
	}

	uint32_t GetInventoryItemCount(const std::vector<RE::TESObjectREFR*>& a_refrArr, RE::TESForm* a_item) {
		uint32_t retVal = 0;

		for (RE::TESObjectREFR* refr : a_refrArr)
			retVal += GetInventoryItemCount(refr, a_item);

		return retVal;
	}

	const RE::TESFile* LookupModByName(RE::TESDataHandler* dataHandler, const std::string& pluginName) {
		for (RE::TESFile* file : dataHandler->files) {
			if (pluginName.compare(file->filename) == 0)
				return file;
		}
		return nullptr;
	}

	enum RecordFlag {
		kNone = 0,
		kESM = 1 << 0,
		kActive = 1 << 3,
		kLocalized = 1 << 7,
		kESL = 1 << 9
	};

	bool IsLight(const RE::TESFile* mod) {
		return (mod->flags & RecordFlag::kESL) == RecordFlag::kESL;
	}

	uint32_t GetPartialIndex(const RE::TESFile* mod) {
		return !IsLight(mod) ? mod->compileIndex : (0xFE000 | mod->smallFileCompileIndex);
	}

	RE::TESForm* GetFormFromIdentifier(const std::string& pluginName, const uint32_t formId) {
		RE::TESDataHandler* g_dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_dataHandler)
			return nullptr;

		const RE::TESFile* mod = LookupModByName(g_dataHandler, pluginName);
		if (!mod || mod->compileIndex == -1)
			return nullptr;

		uint32_t actualFormId = formId;
		uint32_t pluginIndex = GetPartialIndex(mod);
		if (!IsLight(mod))
			actualFormId |= pluginIndex << 24;
		else
			actualFormId |= pluginIndex << 12;

		return RE::TESForm::GetFormByID(actualFormId);
	}

	RE::TESForm* GetFormFromIdentifier(const std::string& pluginName, const std::string& formIdStr) {
		uint32_t formID = std::stoul(formIdStr, nullptr, 16) & 0xFFFFFF;
		return GetFormFromIdentifier(pluginName, formID);
	}
}
