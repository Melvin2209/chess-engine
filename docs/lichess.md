# Running chess-engine as a lichess bot

Lichess lets engines play as **BOT accounts** over the
[Bot API](https://lichess.org/api#tag/Bot). The simplest way to connect a UCI engine is the
official bridge, [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot), which speaks UCI to
the engine and HTTP to lichess.

## 1. Create a bot account

1. Create a **new** lichess account. It must not have played any games, because upgrading to a bot
   account cannot be undone.
2. Generate an API token at <https://lichess.org/account/oauth/token/create> with the scope
   **`bot:play`** (Play games with the bot API).
3. Upgrade the account (one time only):

   ```bash
   curl -d '' https://lichess.org/api/bot/account/upgrade -H "Authorization: Bearer $LICHESS_TOKEN"
   ```

## 2. Build the engine

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 3. Install lichess-bot

```bash
git clone https://github.com/lichess-bot-devs/lichess-bot.git
cd lichess-bot
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
cp config.yml.default config.yml
```

## 4. Configure

Edit `config.yml`. The relevant parts (see [`lichess-bot.config.yml`](lichess-bot.config.yml) for a
complete excerpt):

```yaml
token: "xxxxxxxxxxxxxxxx"          # or export LICHESS_BOT_TOKEN instead of writing it here

engine:
  dir: "/path/to/chess-engine/build/"
  name: "chess-engine"
  protocol: "uci"
  ponder: false                    # pondering is not implemented
  uci_options:
    Hash: 64
    Move Overhead: 300             # network latency margin, in ms

challenge:
  concurrency: 1
  variants: [standard]
  time_controls: [bullet, blitz, rapid]
  modes: [casual, rated]
```

Keep a generous `Move Overhead` online: lichess measures time on its servers, so network lag is
counted against the engine.

## 5. Run

```bash
python3 lichess-bot.py
```

The bot then accepts challenges that match the `challenge` section. Challenge it from another
account at `https://lichess.org/@/<bot-name>`, or enable `matchmaking` in `config.yml` to let it
seek games against other bots.
