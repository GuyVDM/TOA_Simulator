#include <vector>
#include <iostream>
#include <conio.h>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include <ctime>   
#include <numeric>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

#define CLEAR_SCREEN() printf("\033[H\033[J"); 

#define CLAMP(x, lower, upper) ((x) < (lower) ? (lower) : ((x) > (upper) ? (upper) : (x)))
#define NEWLINE() std::cout << RESET << std::endl;
#define PRINT(x)  std::cout << x << RESET << std::endl;

enum class e_UniqueType
{
	NONE = 0,
	RING,
	FANG,
	WARD,
	MASORI_MASK,
	MASORI_BODY,
	MASORI_CHAPS,
	SHADOW
};

struct TOA_Item
{
	static int32_t s_TotalWeight;

	std::string  name;
	int32_t      weight;
	int32_t      reqInvoc;

	TOA_Item(std::string _name, int32_t _weight, int32_t _reqInvoc) 
		: name(_name), weight(_weight), reqInvoc(_reqInvoc)
	{
		s_TotalWeight += weight;
	}

	TOA_Item() :
		name("Null"), weight(0), reqInvoc(0)
	{
		s_TotalWeight += weight;
	}

	inline float GetChancePercentage(float _uniqueRate) 
	{
		return _uniqueRate * ((float)weight / (float)s_TotalWeight);
	}

	inline int32_t GetOneIn(float _uniqueRate) 
	{
		return static_cast<int32_t>(ceilf(100.0f / GetChancePercentage(_uniqueRate)));
	}
};  int32_t TOA_Item::s_TotalWeight = 0;

std::unordered_map<e_UniqueType, TOA_Item> dropTable =
{
	{ e_UniqueType::RING,         { "Lightbearer",      7, 50 }},
	{ e_UniqueType::FANG,         { "Osmumten's Fang",  7, 50 }},
	{ e_UniqueType::WARD,         { "Elidnis' ward",    3, 150}},
	{ e_UniqueType::MASORI_MASK,  { "Masori mask",      2, 150}},
	{ e_UniqueType::MASORI_BODY,  { "Masori body",      2, 150}},
	{ e_UniqueType::MASORI_CHAPS, { "Masori chaps",     2, 150}},
	{ e_UniqueType::SHADOW,       { "Tumeken's shadow", 1, 150}}
};

struct TOA_Settings 
{
	//-----RAID PARAMETERS-----//
	int32_t  invocLvl          = 0;
	int32_t  wardenDownCountP2 = 1;
	bool     bDoSkullSkip    = false;
	bool     bWalkThePath    = false;

	//-----SIMULATION PARAMETERS-----//
	int32_t  simStepSize   = 1;
	int32_t  simStepTime = 1;
};

struct TOA_Unique_Info
{
	e_UniqueType type;
	int32_t      kc;
};

struct TOA_Analytics 
{
	int32_t kc = 0;
	int32_t drystreak = 0;
	int32_t longest_drystreak = 0;
	std::vector<TOA_Unique_Info> uniqueData;
	bool bShowUniqueData = true;

	std::unordered_map<e_UniqueType, int32_t> uniques
	{
		{ e_UniqueType::RING, 0 },
		{ e_UniqueType::FANG, 0 },
		{ e_UniqueType::WARD, 0 },
		{ e_UniqueType::MASORI_MASK, 0 },
		{ e_UniqueType::MASORI_BODY, 0 },
		{ e_UniqueType::MASORI_CHAPS, 0 },
		{ e_UniqueType::SHADOW, 0 }
	};
};

struct TOA_Entity
{
	float   baseHitpoints;
	float   pointMultiplier;
	int32_t entityCount;
	bool    bAffectedByInvoc;
};

static bool RollDice(int32_t _sides, std::mt19937& rng)
{
	int32_t dice = (int)(rng() % _sides + 1);
	return dice == _sides;
}

static float CalculateUniqueChance(int32_t _points, const TOA_Settings& _settings) 
{
	if (_points == 0)
		return 0.0f;

	int32_t x = CLAMP(_settings.invocLvl, 0, 400);
	int32_t y = CLAMP(_settings.invocLvl - 400, 0, 150);

	float calculateY = y > 0 ? (float)y / 3.0f : 0;
	return _points / (10500.0f - 20.0f * ((float)x + calculateY));
}

static bool TryRollUnique(float _chance, e_UniqueType& type, const TOA_Settings& _settings, std::mt19937& rng)
{
	std::uniform_real_distribution<float> dis(0.0f, 100.0f);	
	const float rand = dis(rng);

	if (_chance >= rand) //Check if we succeed
	{
		int32_t dice = (int32_t)(rng() % TOA_Item::s_TotalWeight + 1);

		int accumulatedWeight = 0;	
		for(const auto& item : dropTable) 
		{
			accumulatedWeight += item.second.weight;
			
			if(dice <= accumulatedWeight)
			{
				if (item.second.reqInvoc > _settings.invocLvl)
				{
					// If the required invoc for this item is not met, roll a 1/50 penalty
					if (RollDice(50, rng))
					{
						goto reward_unique;
					}
				}
				else 
				{

				reward_unique:
					type = item.first;
					return true;
				}
			}
		}
	}

	return false;
}

static int32_t CalculateRaidPoints(const TOA_Settings& _settings) 
{
	float TotalRaidPoints = 5000;

	const std::vector<TOA_Entity> Raid_Entities
	{

		//--------BOSSES-------//
		{ 631, 1.5f, 1, true },	  //Zebak
		{ 400, 1.0f, 1, false },  //Palm Watering

		//--------WARDEN-------//
		{ 260, 1.5f, 1, true },	  //Warden_P1 - Obelisk
		{ 140, 2.0f, CLAMP(_settings.wardenDownCountP2, 1, 3), true },//Warden_P2
		{ 880, 2.5f, 1, true },	  //Warden_P3

		//-------AKKHA-------//
		{ 546, 1.0f, 1, true  },  //Akkha
		{ 79,  1.0f, 4, true  },  //Akkha's Shadows 
		{ 130, 2.5f, 1, false },  //Obelisk - Path of Het

		//-------KEPHRI-------//
		{ 138, 1.0f, 1, true },	  //Kephri
		{ 398, 1.0f, 1, true },	  //Kephri's Shield
		{ 40,  0.5f, 1, true },	  //Soldier Scarab
		{ 40,  0.5f, 1, true },	  //Spitting Scarab
		{ 40,  0.5f, 1, true },	  //Arcane Scarab

		//-------BABA-------//
		{ 380, 2.0f, 1, true },   //Baba
		{ 6,   1.2f, 7, true },   //Brawler-Baboon
		{ 6,   1.2f, 6, true },   //Thrower-Baboon
		{ 6,   1.2f, 4, true },   //Mage-Baboon
		{ 16,  1.2f, 9, true },   //Shaman-Baboon
		{ 10,  1.2f, 5, true }    //Cursed-Baboon
		//{ 8,   1.2f, 6, true }    //Volatile-Baboon
	};

	const float invocMultiplier = ((float)(_settings.invocLvl) / 5.0f) * 0.02f;

	for(const auto& entity : Raid_Entities) 
	{
		float points = entity.baseHitpoints;

		points += entity.bAffectedByInvoc ? (entity.baseHitpoints * invocMultiplier) : 0;

		// Apply multipliers
		{
			points *= entity.pointMultiplier;
			points *= entity.entityCount;
		}

		// Apply Walk the Path Invocation
		if(_settings.bWalkThePath && entity.bAffectedByInvoc)
		{
			const float wtp_bonus = floorf(entity.baseHitpoints * 1.08f) - entity.baseHitpoints;
			TotalRaidPoints += wtp_bonus;
		}

		TotalRaidPoints += floorf(points);
	}

	// remove 5% total as that's the estimate of the skull skip if not enabled.
	if(!_settings.bDoSkullSkip) 
	{
		const float skullskipPenalty = (TotalRaidPoints * 0.05f);
		TotalRaidPoints -= skullskipPenalty;
	}


	return (int32_t)floorf(TotalRaidPoints);
}

int main()
{
	TOA_Settings settings;
	settings.invocLvl = 375;
	settings.wardenDownCountP2 = 3;
	settings.bDoSkullSkip = true;
	settings.bWalkThePath = true;
	settings.simStepSize = 1;
	settings.simStepTime = 1;

	TOA_Analytics analytics;
	analytics.bShowUniqueData = true;

	const int32_t raidPoints = CalculateRaidPoints(settings);
	const float uniqueRate   = CalculateUniqueChance(raidPoints, settings);
	
	std::random_device rd;
	auto now = std::chrono::system_clock::now();
	auto seed = static_cast<uint32_t>(now.time_since_epoch().count()) + rd();
	std::mt19937 rng(seed);

	//Last recently gained Unique
	e_UniqueType mostRecentUnique = e_UniqueType::NONE;

	while (true)
	{
		CLEAR_SCREEN();

#pragma region RAID_SETTINGS
		{
			PRINT(YELLOW << "--------- SETTINGS --------");
			PRINT("Warden P2 Downs: " << GREEN << CLAMP(settings.wardenDownCountP2, 1, 3) << "x");
			if (settings.bDoSkullSkip)
			{
				PRINT("Skullskip: " << GREEN << "Enabled");
			}
			else
			{
				PRINT("Skullskip: " << RED << "Disabled");
			}

			if (settings.bWalkThePath)
			{
				PRINT("Walk The Path: " << GREEN << "Enabled");
			}
			else
			{
				PRINT("Walk The Path: " << RED << "Disabled");
			}
			NEWLINE();
		}

#pragma endregion

#pragma region RAID_STATS
		{
			PRINT(YELLOW << "--------- RAID STATS --------");
			PRINT("Invocation Level is: " << GREEN << settings.invocLvl);
			PRINT("Points: " << GREEN << raidPoints);
			PRINT("Unique Chance is: " << GREEN << uniqueRate << "%" << " (1/" << (int)ceilf(100.0f / uniqueRate) << ")");

			NEWLINE();
		}
#pragma endregion

#pragma region RAID_SIM
		PRINT(YELLOW << "--------- RAID SIM --------");
		for (int i = 0; i < settings.simStepSize; i++)
		{
			analytics.kc++;

			if (TryRollUnique(uniqueRate, mostRecentUnique, settings, rng))
			{
				PRINT(GREEN << "You have received a Unique: " << RED << dropTable[mostRecentUnique].name << "!");
				analytics.uniques[mostRecentUnique]++;
				analytics.uniqueData.push_back(TOA_Unique_Info { mostRecentUnique, analytics.kc } );

				if(analytics.longest_drystreak < analytics.drystreak) 
				{
					analytics.longest_drystreak = analytics.drystreak;
				}

				analytics.drystreak = 0;
			}
			else
			{
				analytics.drystreak++;
			}

		}

		PRINT("Raid Killcount is: " << YELLOW << analytics.kc);
		PRINT("Drystreak is: " << YELLOW << analytics.drystreak);
		PRINT("Longest drystreak is: " << YELLOW << analytics.longest_drystreak);
		NEWLINE();
#pragma endregion

 #pragma region RAID_UNIQUES_SIM
		PRINT(YELLOW << "--------- UNIQUES GAINED --------");

		for(const auto& unique : analytics.uniques) 
		{
			const e_UniqueType uniqueType = unique.first;

			TOA_Item& uniqueItem = (*std::find_if(dropTable.begin(), dropTable.end(), [uniqueType](const auto& _a)
				{
					return (_a.first == uniqueType);
				})).second;

			int32_t dropRate =  uniqueItem.GetOneIn(uniqueRate);
			
			// Apply droprate penalty if the min invocation level for the item is not met.
			{
				const int32_t dropratePenalty = 50;

				if (settings.invocLvl < uniqueItem.reqInvoc)
					dropRate *= dropratePenalty;
			}

			// Print the rarity, our personal rate, and the amount of the unqiques we have obtained.
			{
				int32_t simDropRate = (analytics.uniques[uniqueType] != 0 ? (analytics.kc / analytics.uniques[uniqueType]) : dropRate);

				if (uniqueType == mostRecentUnique)
				{
					PRINT(RED << "(1/" << dropRate << ")" << RESET << " vs " << GREEN << "(1/" << simDropRate << ") " << YELLOW << dropTable[unique.first].name << ": " << GREEN << unique.second << "x");
				}
				else
				{
					PRINT(RED << "(1/" << dropRate << ")" << RESET << " vs " << GREEN << "(1/" << simDropRate << ") " << RESET << dropTable[unique.first].name << ": " << GREEN << unique.second << "x");
				}
			}
		}
#pragma endregion

#pragma region RAID_UNIQUES_INFO 
		if (analytics.bShowUniqueData) 
		{
			NEWLINE();
			PRINT(YELLOW << "--------- UNIQUE HISTORY --------");

			const int32_t maxHistorySize = 5;
			while(analytics.uniqueData.size() > maxHistorySize) 
			{
				analytics.uniqueData.erase(analytics.uniqueData.begin());
			}

			for(const auto& entry : analytics.uniqueData) 
			{
				const size_t i = &entry - &analytics.uniqueData[0];

				if (i > 0) 
				{
					PRINT("Received unique: " << GREEN << dropTable[entry.type].name << RESET << " at " << GREEN << entry.kc << "." << YELLOW << "(" << (entry.kc - analytics.uniqueData[i - 1].kc) << " kc dry.)");
				}
				else 
				{
					PRINT("Received unique: " << GREEN << dropTable[entry.type].name << RESET << " at " << GREEN << entry.kc);
				}
			}
		}
#pragma endregion

		std::this_thread::sleep_for(std::chrono::seconds(settings.simStepTime));
	}
}