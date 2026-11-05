#include "ArmoryClient.h"

namespace Core
{
    Task<std::vector<CharacterSummary>> ArmoryClient::FetchCharactersAsync(std::wstring)
    {
        // No realm-specific armory endpoint is confirmed yet -- returns an
        // empty list rather than guessing a URL shape. CharactersPage shows
        // an appropriate empty state (Step 4) rather than an error for this.
        co_return {};
    }
}
