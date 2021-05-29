# OrlodrimBot

## Description

[OrlodrimBot](https://fr.wikipedia.org/wiki/Utilisateur:OrlodrimBot) is a bot that runs on the French Wikipedia.

This repository contains:

* mwclient + cbl, the framework used the bot (mwclient is the MediaWiki library and cbl is a library of generic functions).
* orlodrimbot, for task-specific code.

Both the framework and the bot are distributed under the MIT license. See the LICENSE file for details.

As a start, the initial version contains the [sandbox](https://fr.wikipedia.org/wiki/Aide:Bac_%C3%A0_sable) cleaner. Other tools will be added over time.

## Setup

The only dependency of the framework is libcurl (libcurl4-openssl-dev package in Ubuntu/Debian). The bot itself also depends on sqlite (libsqlite3-dev) and [re2](https://github.com/google/re2).

The code is written in C++20. To build the framework library and the bot tools, run:

`make`

To use any of the tools, you need to create a login file. Use this template:

```
url=https://fr.wikipedia.org/w
userName=<your username on Wikipedia>
password=<your password on Wikipedia>
userAgent=http://fr.wikipedia.org/wiki/Utilisateur:<your username on Wikipedia>
delayBeforeRequests=1
```

Then, run the tool with `--loginfile=<path of the login file>`. For instance:

```
./orlodrimbot/sandbox/sandbox --loginfile=idwp.txt
```
