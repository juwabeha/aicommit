# aicommit

A tiny command-line tool that writes your git commit messages for you.
It reads your staged changes (`git diff --staged`), sends them to Anthropic's
Claude API, and proposes a commit message in
[Conventional Commits](https://www.conventionalcommits.org/) format. You review
it and decide whether to commit.

## Features

- Generates concise commit messages from staged changes
- Conventional Commits style (`feat` / `fix` / `refactor` / `docs` / `chore`)
- Configurable language, model, token limit, and API key
- Asks for confirmation before committing — nothing happens without your `Y`
- Config stored locally with owner-only permissions (`0600`)

## Requirements

- A POSIX system (macOS or Linux) — Windows is not supported
- A C++ compiler with C++23 support (`std::format`, `std::filesystem`)
- [CMake](https://cmake.org/) 3.20 or newer
- [libcurl](https://curl.se/libcurl/)
- [nlohmann/json](https://github.com/nlohmann/json)
- An [Anthropic API key](https://console.anthropic.com/)

### Installing dependencies

**macOS (Homebrew):**

```sh
brew install cmake curl nlohmann-json
```

**Debian / Ubuntu:**

```sh
sudo apt install cmake libcurl4-openssl-dev nlohmann-json3-dev
```

## Build

```sh
git clone <your-repo-url> aicommit
cd aicommit
cmake -S . -B build
cmake --build build
```

The binary is produced at `build/aicommit`. Copy it somewhere on your `PATH`
if you want to call it from anywhere:

```sh
sudo cp build/aicommit /usr/local/bin/
```

## First run

The first time you run `aicommit`, it asks for a few settings and stores them
in `~/.config/aicommit/config`:

```
$ aicommit
Enter Anthropic model's name
claude-sonnet-4-5
Enter your API key
sk-ant-...
Enter language for commit texts (e.g. English, Russian, ru, en)
en
```

The config file is created with `0600` permissions so only your user can read
the API key.

## Usage

Stage your changes, then run `aicommit`:

```sh
git add .
aicommit
```

You'll see a proposed commit message and a prompt:

```
feat: add user authentication flow

Commit? (Y/n)
```

Type `Y` (or `y`, `yes`) to create the commit, anything else to cancel.

### Flags

| Flag | Alias | Description |
| --- | --- | --- |
| `-h` | `--help` | Show the help guide |
| `-l VALUE` | `--language VALUE` | Change the commit message language |
| `-k VALUE` | `--key VALUE` | Change the Claude API key |
| `-t VALUE` | `--tokens VALUE` | Change the max tokens per request |
| `-m VALUE` | `--model VALUE` | Change the Claude model name |

Each setter flag updates the stored config and exits, for example:

```sh
aicommit -l russian
aicommit -m claude-opus-4
aicommit -t 2048
```

## Configuration

Settings live in `~/.config/aicommit/config` as four plain-text lines:

```
<model name>
<api key>
<max tokens>
<language>
```

You can edit them with the flags above or by hand.

## How it works

1. Runs `git diff --staged` to collect your staged changes.
2. Builds a request to `https://api.anthropic.com/v1/messages` with your model,
   token limit, and a prompt asking for a Conventional Commits message.
3. Prints the returned message and asks for confirmation.
4. On confirmation, writes the message to a temporary file and runs
   `git commit -F <file>` (using a file avoids any shell-escaping issues with
   the generated text).

## License

[MIT](LICENSE)
