#include <pl/pattern_language.hpp>
#include <nlohmann/json.hpp>
#include <cinttypes>

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_unsigned.hpp>

static std::string consoleResult;
static std::vector<pl::u8> loadedData;

static std::vector<const pl::ptrn::Pattern *> propertyPatterns;
static std::string uiConfig;

static pl::PatternLanguage runtime;

extern "C" void initialize() {
    runtime.setDangerousFunctionCallHandler([]() {
        return false;
    });

    runtime.setIncludePaths({
            "/sources/includes",
            "/sources/patterns"
    });
}

extern "C" const char *getConsoleResult() {
    return consoleResult.c_str();
}

extern "C" const char *getUIConfig() {
    return uiConfig.c_str();
}

extern "C" void setData(pl::u8 *data, size_t size) {
    loadedData = std::vector<pl::u8>(data, data + size);

    runtime.setDataSource(0x00, loadedData.size(), [&](pl::u64 address, void *buffer, size_t size) {
        std::memcpy(buffer, loadedData.data() + address, size);
    });
}

static std::string generateUIJson(const std::vector<const pl::ptrn::Pattern *> &patterns) {
    nlohmann::json json = nlohmann::json::array();

    std::map<std::string, std::map<std::string, pl::u64>> categorizedPatterns;
    pl::u64 index = 0;
    for (const auto pattern : patterns) {
        const auto propertyArgs = pattern->getAttributeArguments("property");
        if (propertyArgs.size() != 2) continue;

        categorizedPatterns[propertyArgs[0].toString()][propertyArgs[1].toString()] = index;
        index += 1;
    }

    for (const auto &[category, rest] : categorizedPatterns) {
        nlohmann::json categoryJson;
        categoryJson["categoryName"] = category;
        categoryJson["items"] = nlohmann::json::array();
        for (const auto &[name, patternIndex] : rest) {
            nlohmann::json itemJson;
            itemJson["name"] = name;
            itemJson["id"] = patternIndex;
            itemJson["properties"] = nlohmann::json::object();

            const auto &pattern = patterns[patternIndex];
            if (const auto uintPattern = dynamic_cast<const pl::ptrn::PatternUnsigned *>(pattern)) {
                itemJson["type"] = "unsigned";
                itemJson["properties"]["min"] = 0U;
                itemJson["properties"]["max"] = (1LLU << (uintPattern->getSize() * 8)) - 1;
            } else if (const auto intPattern = dynamic_cast<const pl::ptrn::PatternSigned *>(pattern)) {
                itemJson["type"] = "signed";
                itemJson["properties"]["min"] = -(1LL << (intPattern->getSize() * 8 - 1));
                itemJson["properties"]["max"] =  (1LL << (intPattern->getSize() * 8 - 1)) - 1;
            } else if (const auto floatPattern = dynamic_cast<const pl::ptrn::PatternFloat *>(pattern)) {
                std::ignore = floatPattern;
                itemJson["type"] = "float";
            } else if (const auto stringPattern = dynamic_cast<const pl::ptrn::PatternString *>(pattern)) {
                itemJson["type"] = "string";
                itemJson["properties"]["length"] = stringPattern->getSize();
            } else if (const auto enumPattern = dynamic_cast<const pl::ptrn::PatternEnum *>(pattern)) {
                itemJson["type"] = "enum";
                itemJson["properties"]["fields"] = nlohmann::json::array();

                for (const auto &[name, values] : enumPattern->getEnumValues()) {
                    auto enumEntry = nlohmann::json::object();
                    enumEntry["name"] = name;
                    enumEntry["value"] = values.min.toUnsigned();

                    itemJson["properties"]["fields"].push_back(enumEntry);
                }
            }

            categoryJson["items"].push_back(itemJson);
        }

        json.push_back(categoryJson);
    }

    return json.dump();
}

extern "C" void executePatternLanguageCode(const char *string) {
    consoleResult.clear();

    runtime.setLogCallback([](auto level, const std::string &message) {
        switch (level) {
            using enum pl::core::LogConsole::Level;

            case Debug:
                consoleResult += fmt::format("[DEBUG] {}\n", message);
                break;
            case Info:
                consoleResult += fmt::format("[INFO]  {}\n", message);
                break;
            case Warning:
                consoleResult += fmt::format("[WARN]  {}\n", message);
                break;
            case Error:
                consoleResult += fmt::format("[ERROR] {}\n", message);
                break;
        }

        consoleResult += '\x01';
    });

    propertyPatterns.clear();
    uiConfig.clear();

    try {
        printf("Executing code \"%s\"", string);
        if (!runtime.executeString(string, "<Source Code>")) {
            if (const auto &compileErrors = runtime.getCompileErrors(); !compileErrors.empty()) {
                for (const auto &error : compileErrors) {
                    consoleResult += fmt::format("{}\n\x01", error.format());
                }
            } else if (const auto &evalError = runtime.getEvalError(); evalError.has_value()) {
                consoleResult += fmt::format("{}:{}  {}\n\x01", evalError->line, evalError->column, evalError->message);
            }
        }
    } catch (const std::exception &e) {
        consoleResult += fmt::format("[ERROR]: Exception thrown: {}\n", e.what());
        consoleResult += '\x01';

        return;
    }

    {
        auto patterns = runtime.getPatternsWithAttribute("property");
        std::copy(patterns.begin(), patterns.end(), std::back_inserter(propertyPatterns));
    }

    printf("%" PRIu64 " properties produced!\n", pl::u64(propertyPatterns.size()));
    uiConfig = generateUIJson(propertyPatterns);
}

int main() {
    fmt::print("Pattern Language Module loaded!\n");
}
