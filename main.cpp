#include <iostream>
#include <string>
#include <format>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>
#include <unistd.h>

std::optional<std::string> exec(const std::string &cmd) {
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return std::nullopt;
    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

size_t WriteCallback(void *ptr, size_t size, size_t nmemb, std::string *out) {
    out->append(static_cast<char *>(ptr), size * nmemb);
    return size * nmemb;
}

struct Config {
    std::string model;
    std::string key;
    std::string tokens;
    std::string lang;
};

std::optional<Config> readConfig(const std::string &path) {
    std::ifstream cfg(path);
    if (!cfg.is_open()) return std::nullopt;
    Config c;
    if (!std::getline(cfg, c.model)) return std::nullopt;
    if (!std::getline(cfg, c.key)) return std::nullopt;
    if (!std::getline(cfg, c.tokens)) return std::nullopt;
    if (!std::getline(cfg, c.lang)) return std::nullopt;
    return c;
}

bool writeConfig(const std::string &path, const Config &c) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << c.model << std::endl;
    out << c.key << std::endl;
    out << c.tokens << std::endl;
    out << c.lang << std::endl;
    out.close();
    std::error_code ec;
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        ec);
    return true;
}

void printHelp() {
    std::cout << "How to use \"aicommit\":\n"
            "  (no flag)            - generate a commit message from staged changes\n"
            "  -h / --help          - show this guide\n"
            "  -l / --language VAL  - change commit text language\n"
            "  -k / --key VAL       - change Claude API key\n"
            "  -t / --tokens VAL    - change max tokens per request\n"
            "  -m / --model VAL     - change Claude model name\n";
}

int main(int argc, char *argv[]) {
    std::vector<std::string> args(argv, argv + argc);

    if (args.size() >= 2 && (args[1] == "-h" || args[1] == "--help")) {
        printHelp();
        return 0;
    }

    const char *home = std::getenv("HOME");
    if (!home) {
        std::cerr << "Environment variable not set" << std::endl;
        return 1;
    }
    std::string path = std::format("{}/.config/aicommit/config", home);

    if (!std::filesystem::exists(path)) {
        std::cout << "Enter Anthropic model's name" << std::endl;
        std::string name;
        std::cin >> name;
        std::cout << "Enter your API key" << std::endl;
        std::string key;
        std::cin >> key;
        std::cout << "Enter language for commit texts (e.g. English, Russian, ru, en)" << std::endl;
        std::string language;
        std::cin >> language;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        if (!writeConfig(path, {name, key, "1024", language})) {
            std::cerr << "Config file error" << std::endl;
            return 1;
        }
    }

    if (args.size() >= 2 && !args[1].empty() && args[1][0] == '-') {
        const std::string &flag = args[1];
        if (args.size() != 3) {
            std::cerr << "Option " << flag << " requires exactly one value" << std::endl;
            std::cerr << "Run 'aicommit --help' for usage" << std::endl;
            return 1;
        }
        const std::string &value = args[2];

        auto cfgOpt = readConfig(path);
        if (!cfgOpt) {
            std::cerr << "Config file error" << std::endl;
            return 1;
        }
        Config cfg = *cfgOpt;

        if (flag == "-l" || flag == "--language") {
            cfg.lang = value;
        } else if (flag == "-k" || flag == "--key") {
            cfg.key = value;
        } else if (flag == "-m" || flag == "--model") {
            cfg.model = value;
        } else if (flag == "-t" || flag == "--tokens") {
            try {
                std::stoi(value);
            } catch (const std::exception &) {
                std::cerr << "Tokens value must be a number" << std::endl;
                return 1;
            }
            cfg.tokens = value;
        } else {
            std::cerr << "Unknown option: " << flag << std::endl;
            std::cerr << "Run 'aicommit --help' for usage" << std::endl;
            return 1;
        }

        if (!writeConfig(path, cfg)) {
            std::cerr << "Config file error" << std::endl;
            return 1;
        }
        std::cout << "Config updated" << std::endl;
        return 0;
    }

    auto cfgOpt = readConfig(path);
    if (!cfgOpt) {
        std::cerr << "Config file error" << std::endl;
        return 1;
    }
    Config cfg = *cfgOpt;

    std::string diff =
            std::format(
                "You are a helpful assistant that writes git commit messages. Write a concise commit message in this language: ({}) for the following git diff. Use conventional commits format (feat/fix/refactor/docs/chore). Return only the commit message, nothing else.\n\n",
                cfg.lang);

    std::optional<std::string> result = exec("git diff --staged");
    if (result == std::nullopt) {
        std::cerr << "Failed to run git diff" << std::endl;
        return 1;
    }
    if (result.value().empty()) {
        std::cerr << "Nothing staged to commit" << std::endl;
        return 1;
    }
    diff.append(result.value());
    int tokens;
    try {
        tokens = std::stoi(cfg.tokens);
    } catch (const std::exception &) {
        std::cerr << "Invalid max_tokens in config" << std::endl;
        return 1;
    }
    nlohmann::json body_json = {
        {"model", cfg.model},
        {"max_tokens", tokens},
        {"messages", {{{"role", "user"}, {"content", diff}}}}
    };
    std::string body = body_json.dump();

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL error" << std::endl;
        return 1;
    }
    std::string response;
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, std::format("x-api-key: {}", cfg.key).c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
        return 1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(response);
    } catch (const std::exception &e) {
        std::cerr << "Failed to parse API response: " << e.what() << std::endl;
        return 1;
    }
    if (!json.contains("content")) {
        std::cerr << "API error: " << response << std::endl;
        return 1;
    }
    std::string msg;
    try {
        msg = json["content"].at(0).at("text").get<std::string>();
    } catch (const std::exception &e) {
        std::cerr << "Unexpected API response format: " << response << std::endl;
        return 1;
    }
    std::cout << msg << std::endl << std::endl;
    std::cout << "Commit? (Y/n)" << std::endl;
    std::string inp;
    std::cin >> inp;
    if (inp == "Y" || inp == "y" || inp == "Yes" || inp == "yes" || inp == "YES") {
        std::filesystem::path tmp = std::filesystem::temp_directory_path() /
                                    std::format("aicommit_msg_{}.txt", getpid()); {
            std::ofstream f(tmp);
            if (!f.is_open()) {
                std::cerr << "Failed to write commit message file" << std::endl;
                return 1;
            }
            f << msg;
        }
        auto com = exec(std::format("git commit -F \"{}\"", tmp.string()));
        std::filesystem::remove(tmp);
        if (!com) {
            std::cerr << "git commit failed" << std::endl;
            return 1;
        }
    }
    return 0;
}
