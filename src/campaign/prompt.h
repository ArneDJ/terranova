
enum class TownPromptChoice {
	UNDECIDED,
	BESIEGE,
	VISIT,
	REST,
	TRANSFER,
	CANCEL
};

// TODO find a better way to implement prompts
struct TownPrompt {
	enum TownPromptChoice choice = TownPromptChoice::UNDECIDED;
	bool active = false;
};
