#pragma once

namespace Utils {
	class WorkbenchMenuBase : public RE::GameMenuBase
	{
	public:
		RE::TESObjectREFR* unkE0;					// E0
		RE::TESObjectREFR* unkE8;					// E8
		uint64_t unkF0;								// F0
		uint64_t unkF8;								// F8
		uint64_t unk100;							// 100
		void* unk108;								// 108
		RE::Inventory3DManager inventory3DManager;	// 110
		RE::BGSInventoryList inventoryList;			// 250
	};

	WorkbenchMenuBase* GetCraftingMenu() {
		RE::UI* g_ui = RE::UI::GetSingleton();
		if (!g_ui)
			return nullptr;

		for (auto menuPtr : g_ui->menuStack) {
			RE::IMenu* menu = menuPtr.get();
			if (menu && (menu->menuName == "ExamineMenu" || menu->menuName == "CookingMenu" || menu->menuName == "RobotModMenu"))
				return reinterpret_cast<WorkbenchMenuBase*>(menu);
		}
		return nullptr;
	}

	uint32_t GetStackCount(RE::BGSInventoryItem::Stack* a_stack) {
		RE::BGSInventoryItem::Stack* sp = a_stack;
		uint32_t itemCnt = 0;
		while (sp) {
			itemCnt += sp->count;
			sp = sp->nextStack.get();
		}
		return itemCnt;
	}

	uint32_t GetItemCount(RE::BGSInventoryList* a_inv, RE::TESForm* a_item) {
		if (!a_inv || !a_item)
			return 0;

		uint32_t totalItemCount = 0;
		a_inv->rwLock.lock_read();
		for (RE::BGSInventoryItem& item : a_inv->data) {
			if (item.object == a_item) {
				totalItemCount = GetStackCount(item.stackData.get());
				break;
			}
		}
		a_inv->rwLock.unlock_read();

		return totalItemCount;
	}

	uint32_t GetComponentCount(RE::BSTArray<RE::BSTTuple<RE::TESForm*, RE::BGSTypedFormValuePair::SharedVal>>* componentData, RE::BGSComponent* a_cmpo) {
		if (!componentData)
			return 0;

		for (auto tuple : *componentData) {
			if (tuple.first == a_cmpo)
				return tuple.second.i;
		}

		return 0;
	}

	uint32_t GetComponentCount(RE::BGSInventoryList* a_inv, RE::BGSComponent* a_cmpo) {
		if (!a_inv || !a_cmpo)
			return 0;

		uint32_t totalItemCount = 0;
		a_inv->rwLock.lock_read();
		for (RE::BGSInventoryItem& item : a_inv->data) {
			if (!item.object || item.object->formType != RE::ENUM_FORM_ID::kMISC)
				continue;

			RE::TESObjectMISC* miscItem = RE::fallout_cast<RE::TESObjectMISC*, RE::TESForm>(item.object);
			if (!miscItem)
				continue;

			if (miscItem == a_cmpo->scrapItem)
				totalItemCount += GetStackCount(item.stackData.get());
			else {
				uint32_t compCnt = GetComponentCount(miscItem->componentData, a_cmpo);
				if (compCnt == 0)
					continue;

				totalItemCount += GetStackCount(item.stackData.get()) * compCnt;
			}
		}
		a_inv->rwLock.unlock_read();

		return totalItemCount;
	}

	uint32_t GetInventoryItemCount(RE::BGSInventoryList* a_inv, RE::TESForm* a_item) {
		if (a_item->formType == RE::ENUM_FORM_ID::kCMPO)
			return GetComponentCount(a_inv, RE::fallout_cast<RE::BGSComponent*, RE::TESForm>(a_item));
		else
			return GetItemCount(a_inv, a_item);
	}

	uint32_t GetInventoryItemCount(RE::TESObjectREFR* a_refr, RE::TESForm* a_item) {
		if (!a_refr || !a_item)
			return 0;

		RE::BGSInventoryList* inventory = a_refr->inventoryList;
		if (!inventory)
			return 0;

		return GetInventoryItemCount(inventory, a_item);
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
