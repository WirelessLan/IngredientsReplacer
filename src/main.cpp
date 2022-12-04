#include <fstream>
#include <xbyak/xbyak.h>

#include "Utils.h"

std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::unordered_set<RE::TESForm*>>> g_replacerMap;

class ConfigReader {
public:
	std::string ConfigPath;

	ConfigReader(const std::string& path) : ConfigPath(path), configReadTime(0) {}

	void ReadConfig() {
		g_replacerMap.clear();

		std::fstream configFile(ConfigPath);
		if (!configFile.is_open()) {
			logger::warn(FMT_STRING("Cannot read the config file! - {}"), ConfigPath);
			return;
		}

		std::string recipePluginName, recipeFormIdStr, targetIngredientPluginName, targetIngredientFormIdStr,
			replaceIngredientPluginName, replaceIngredientFormIdStr;
		while (std::getline(configFile, line)) {
			lineIndex = 0;

			trim(line);
			if (line.empty() || line[0] == '#')
				continue;

			recipePluginName = getNextData('|');
			if (recipePluginName.empty()) {
				logger::warn(FMT_STRING("Cannot read a recipe's plugin name! - {}"), line);
				continue;
			}

			recipeFormIdStr = getNextData('-');
			if (recipeFormIdStr.empty()) {
				logger::warn(FMT_STRING("Cannot read a recipe's formID! - {}"), line);
				continue;
			}

			RE::TESForm* recipeForm = Utils::GetFormFromIdentifier(recipePluginName, recipeFormIdStr);
			if (!recipeForm) {
				logger::warn(FMT_STRING("Cannot find a replace's form! - {}"), line);
				continue;
			}

			targetIngredientPluginName = getNextData('|');
			if (targetIngredientPluginName.empty()) {
				logger::warn(FMT_STRING("Cannot read a target ingredient's plugin name! - {}"), line);
				continue;
			}

			targetIngredientFormIdStr = getNextData('=');
			if (targetIngredientFormIdStr.empty()) {
				logger::warn(FMT_STRING("Cannot read a target ingredient's formID! - {}"), line);
				continue;
			}

			RE::TESForm* targetIngredientForm = Utils::GetFormFromIdentifier(targetIngredientPluginName, targetIngredientFormIdStr);
			if (!recipeForm) {
				logger::warn(FMT_STRING("Cannot find a target ingredient's form! - {}"), line);
				continue;
			}

			replaceIngredientPluginName = getNextData('|');
			if (replaceIngredientPluginName.empty()) {
				logger::warn(FMT_STRING("Cannot read a replace ingredient's plugin name! - {}"), line);
				continue;
			}

			replaceIngredientFormIdStr = getNextData(0);
			if (replaceIngredientFormIdStr.empty()) {
				logger::warn(FMT_STRING("Cannot read a replace ingredient's formID! - {}"), line);
				continue;
			}

			RE::TESForm* replaceIngredientForm = Utils::GetFormFromIdentifier(replaceIngredientPluginName, replaceIngredientFormIdStr);
			if (!replaceIngredientForm) {
				logger::warn(FMT_STRING("Cannot find a replace ingredient's form! - {}"), line);
				continue;
			}

			auto recipe_iter = g_replacerMap.find(recipeForm->formID);
			if (recipe_iter == g_replacerMap.end()) {
				auto recipeInsert_iter = g_replacerMap.insert(std::make_pair(recipeForm->formID, std::unordered_map<uint32_t, std::unordered_set<RE::TESForm*>>()));
				if (!recipeInsert_iter.second) {
					logger::warn("Failed to insert recipe!"sv);
					continue;
				}
				
				recipe_iter = recipeInsert_iter.first;
			}

			auto targetIngredient_iter = recipe_iter->second.find(targetIngredientForm->formID);
			if (targetIngredient_iter == recipe_iter->second.end()) {
				auto targetIngredientInsert_iter = recipe_iter->second.insert(std::make_pair(targetIngredientForm->formID, std::unordered_set<RE::TESForm*>()));
				if (!targetIngredientInsert_iter.second) {
					logger::warn("Failed to insert target ingredient!"sv);
					continue;
				}

				targetIngredient_iter = targetIngredientInsert_iter.first;
			}

			targetIngredient_iter->second.insert(replaceIngredientForm);
		}
	}

	bool ShouldReadConfig() {
		struct _stat64 stat;
		if (_stat64(ConfigPath.data(), &stat) != 0)
			return false;

		if (configReadTime == 0 || configReadTime != stat.st_mtime) {
			configReadTime = stat.st_mtime;
			return true;
		}

		return false;
	}

private:
	std::string line;
	uint32_t lineIndex;
	time_t configReadTime;

	std::string getNextData(char delimeter) {
		char ch;
		std::string retVal = "";

		while ((ch = getNextChar()) > 0) {
			if (ch == '#') {
				undoGetNextChar();
				break;
			}

			if (delimeter != 0 && ch == delimeter)
				break;

			retVal += ch;
		}

		trim(retVal);
		return retVal;
	}

	char getNextChar() {
		if (lineIndex < line.length())
			return line[lineIndex++];

		return -1;
	}

	void undoGetNextChar() {
		if (lineIndex > 0)
			lineIndex--;
	}

	void trim(std::string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
			}));
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
			}).base(), s.end());
	}
};

std::string g_configPath = fmt::format(FMT_STRING("Data\\F4SE\\Plugins\\{}.cfg"), Version::PROJECT);
ConfigReader g_configReader(g_configPath);

template<std::uint64_t id, std::ptrdiff_t offset>
class SetIngredients {
public:
	static void Install() {
		REL::Relocation<std::uintptr_t> target{ REL::ID(id) };

		struct asm_code : Xbyak::CodeGenerator {
			asm_code(std::uintptr_t a_dst) {
				Xbyak::Label dst;

				mov(ptr[rsp + 0x08], rbx);

				jmp(ptr[rip + dst]);

				L(dst);
				dq(a_dst);
			}
		};

		asm_code p{ target.address() + offset };

		auto& trampoline = F4SE::GetTrampoline();
		func = (decltype(&thunk))trampoline.allocate(p);
		trampoline.write_branch<5>(target.address(), thunk);
	}

private:
	struct Ingredient {
		RE::TESForm* form;
		uint32_t count;
	};

	static Ingredient* GetIngredient(RE::BSTArray<Ingredient>* a_arr, RE::TESForm* form) {
		return GetIngredientByFormID(a_arr, form->formID);
	}

	static Ingredient* GetIngredientByFormID(RE::BSTArray<Ingredient>* a_arr, uint32_t formId) {
		auto it = std::find_if(
			a_arr->begin(), a_arr->end(), [&](const Ingredient& val) { return val.form->formID == formId; });
		if (it != a_arr->end())
			return it;
		return nullptr;
	}

	static void RemoveIngredient(RE::BSTArray<Ingredient>* a_arr, RE::TESForm* form) {
		RemoveIngredientByFormID(a_arr, form->formID);
	}

	static void RemoveIngredientByFormID(RE::BSTArray<Ingredient>* a_arr, uint32_t formId) {
		auto it = std::remove_if(
			a_arr->begin(), a_arr->end(), [&](const Ingredient& val) { return val.form->formID == formId; });
		if (it == a_arr->end())
			return;
		a_arr->erase(it, a_arr->end());
	}

	static void thunk(RE::BGSConstructibleObject* a_this, RE::BSTArray<Ingredient>** a_ingArr, uint32_t arg3 = 0) {
		func(a_this, a_ingArr, arg3);

		if (!*a_ingArr)
			return;

		auto conObj_iter = g_replacerMap.find(a_this->formID);
		if (conObj_iter == g_replacerMap.end())
			return;

		std::vector<Ingredient> targetIngVec;
		for (Ingredient ingredient : **a_ingArr) {
			auto ing_iter = conObj_iter->second.find(ingredient.form->formID);
			if (ing_iter == conObj_iter->second.end())
				continue;

			targetIngVec.push_back(ingredient);
		}

		RE::PlayerCharacter* g_player = RE::PlayerCharacter::GetSingleton();
		if (!g_player)
			return;

		std::vector<RE::TESObjectREFR*> refrVec;
		refrVec.push_back(g_player);

		RE::TESObjectREFR* currFurnRefr = Utils::GetCurrentFurniture(g_player);
		if (currFurnRefr)
			refrVec.push_back(currFurnRefr);
		RE::TESObjectREFR* workshopRefr = currFurnRefr ? Utils::GetParentWorkshop(currFurnRefr) : nullptr;
		if (workshopRefr)
			refrVec.push_back(workshopRefr);

		for (Ingredient ingredient : targetIngVec) {
			uint32_t ingHoldCnt = Utils::GetInventoryItemCount(refrVec, ingredient.form);
			if (ingHoldCnt >= ingredient.count)
				continue;

			auto ing_iter = conObj_iter->second.find(ingredient.form->formID);
			if (ing_iter == conObj_iter->second.end())
				continue;

			uint32_t reqCnt = ingredient.count - ingHoldCnt;
			for (RE::TESForm* form : ing_iter->second) {
				uint32_t formHoldCnt = Utils::GetInventoryItemCount(refrVec, form);
				if (formHoldCnt >= reqCnt) {
					Ingredient* formIng = GetIngredient(*a_ingArr, form);
					if (formIng)
						formIng->count += reqCnt;
					else 
						(*a_ingArr)->push_back({ form, reqCnt });
					reqCnt = 0;
					break;
				}
			}

			if (reqCnt > 0 && reqCnt != ingredient.count) {
				Ingredient* arrIng = GetIngredient(*a_ingArr, ingredient.form);
				if (arrIng)
					arrIng->count -= reqCnt;
			}
			else if (reqCnt == 0) {
				RemoveIngredient(*a_ingArr, ingredient.form);
			}
		}
	}

	inline static decltype(&thunk) func;
};

void Install() {
	SetIngredients<905300, 0x05>::Install();
}

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg) {
	switch (msg->type) {
	case F4SE::MessagingInterface::kNewGame:
	case F4SE::MessagingInterface::kPreLoadGame:
		if (g_configReader.ShouldReadConfig())
			g_configReader.ReadConfig();
		break;
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface * a_f4se, F4SE::PluginInfo * a_info) {
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical(FMT_STRING("unsupported runtime v{}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface * a_f4se) {
	F4SE::Init(a_f4se);
	F4SE::AllocTrampoline(1u << 10);

	Install();

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	if (message)
		message->RegisterListener(OnF4SEMessage);

	return true;
}
