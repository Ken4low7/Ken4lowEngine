#include <cstdint>
#include <windows.h>

#include "MeshConverter.h"

enum class CommandLineArgument : std::uint8_t
{
	kApplicationPath,
	kFilePath,
	NumArguments
};

int main(int argc, char* argv[])
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	if (argc < static_cast<int>(CommandLineArgument::NumArguments))
	{
		MeshConverter::OutputUsage();
		return 1;
	}

	MeshConverter conv;

	const int numOptions = argc - static_cast<int>(CommandLineArgument::NumArguments);
	char** options = argv + static_cast<int>(CommandLineArgument::NumArguments);

	const bool ok = conv.ConvertModelToBinary(
		argv[static_cast<uint8_t>(CommandLineArgument::kFilePath)],
		numOptions,
		options);

	return ok ? 0 : 1;
}