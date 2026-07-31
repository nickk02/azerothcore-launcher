#pragma once
#include <string>
#include <vector>
#include "Async.h"

namespace Core
{
    struct CharacterSummary { std::wstring Name; uint32_t Level = 0; std::wstring Class; std::wstring Race; };

    struct ArmoryClient
    {
        static Task<std::vector<CharacterSummary>> FetchCharactersAsync(std::wstring accountName);
    };
}
