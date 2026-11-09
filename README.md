# Cambio — Complete Online C++ Project

A C++17 implementation of the Cambio rules with:

- authoritative game engine
- hidden-information-safe multiplayer server
- browser UI over WebSockets
- 2–4 player rooms
- initial memorization phase
- draw / replace / discard flow
- numeric card display (`1`–`13`, `-1`, `0`)
- special-card powers
- click-to-flip gameplay
- flip penalties and successful-flip handling
- Cambio final round
- observer-specific hidden-card knowledge
- Monte Carlo AI and benchmark

## Card faces

Every card shows its **letter at the top left** (A, 2–10, J, Q, K, JKR) and its **point value at the bottom right**.

- A = 1, 2–10 = face value, J = 11, Q = 12
- Red K = −1, Black K = 13
- Joker = 0 (two in the deck)

Red cards are drawn in red. Cards you peek at (or place) are shown for a few seconds and then turn face-down again, so you have to remember them. At the end of the game every hand is revealed with its point total.

## Game flow

1. At least two players join the same room. Player 0 is the host and starts the game.
2. Each player sees two of their four cards (the bottom row) and memorizes them, then presses **Ready x / n**. All cards then turn face-down.
3. On your turn, draw from the deck or the discard pile (or **Call Cambio** before drawing).
4. After drawing you can **discard** the card to use its power, or **replace** one of your cards with it. A card drawn from the discard pile can only replace.
5. Powers: 7/8 peek at one of your own cards; 9/10 peek at someone else's card; J/Q swap any two cards without looking; **K looks at two cards of your choice, shows them to you, and then you choose Swap or Keep**.
6. Peeked cards stay visible for a few seconds and then turn face-down again.
7. Any player can tap a card to flip it whenever a discard exists. Every flip is public: all players see the flipped card for a moment. A correct flip sends that card to the discard pile (your own card shrinks your hand; an opponent's card also lets you give them one of yours in its place). A wrong flip leaves the card where it is and adds a random face-down penalty card from the deck to the flipper. More than six cards loses. Only one flip can succeed per discard, and you cannot flip on your own discard.
8. After Cambio is called everyone else gets one last turn. The caller's cards cannot be touched and the caller cannot flip.
9. Fewest points wins. The caller must be **strictly** lowest; if anyone ties or beats them, the other player wins. All hands are revealed with their totals at the end.

The discard pile starts **empty**; the first discard comes from the first resolved draw.

### Client/server actions

`CONFIRM_READY`, `DRAW_FROM_DECK`, `DRAW_FROM_DISCARD`, `REPLACE_CARD_WITH_DRAWN`, `DISCARD_DRAWN_CARD`, `PEEK_OWN_CARD`, `PEEK_OPPONENT_CARD`, `BLIND_SWAP`, `LOOK_TWO_CARDS` + `DECIDE_SWAP` (King, two steps), `FLIP_CARD`, `GIVE_CARD_AFTER_FLIP`, `CALL_CAMBIO`.
`LOOK_AND_OPTIONALLY_SWAP` is the one-step form of the King power, kept for the AI. It looks at both cards and swaps only if `shouldSwap` is true, so the result is identical to the two-step flow.

## Build on macOS

Install Boost if necessary:

```bash
brew install boost
```

Then:

```bash
cd cambio-complete
rm -rf build
cmake -S . -B build -DBoost_DIR=$(brew --prefix boost)/lib/cmake/Boost-1.92.0
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If your Boost version is different, replace the version in `Boost_DIR` with the directory printed by:

```bash
brew --prefix boost
```

## Run the server

Terminal 1:

```bash
cd cambio-complete
./build/cambio_server 8080
```

## Run the browser client

Terminal 2:

```bash
cd cambio-complete/web
python3 -m http.server 5173
```

Open:

```text
http://localhost:5173
```

Open two browser windows/tabs, enter different names, and use the same room code. Once both players are present, the host sees **Start game**.

## AI benchmark

```bash
./build/cambio_ai_benchmark 100 1000
```

The current AI is a first Monte Carlo hidden-state policy. The architecture supports deeper rollouts, opponent modelling, belief updates, and statistical benchmarking later.


## Running it (one port)

The server now serves the web page **and** the game WebSocket on the same port.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cambio_server            # then open http://localhost:8080
```

Port: first argument, or the `PORT` environment variable, default 8080. Run it from the project folder (or pass the web folder as the second argument, or set `CAMBIO_WEB_DIR`).

### Playing with other devices

- **Same Wi-Fi:** open `http://<your-computer-ip>:8080` on the other devices.
- **Over the internet (testing):** run a tunnel to the server, for example `cloudflared tunnel --url http://localhost:8080` or `ngrok http 8080`, and share the https link it prints. The page automatically uses `wss://` when loaded over https.
- **Hosted:** deploy the included `Dockerfile` to any container host (Render, Fly.io, Railway). The platform sets `PORT`. Game rooms live in memory, so run a single instance.

If the page is served from somewhere else, add `?server=host:port` to the URL to point it at the game server.


## Deploying as a website (Render)

1. Create a GitHub repository and push this folder: `git init && git add . && git commit -m "Cambio" && git branch -M main && git remote add origin <your-repo-url> && git push -u origin main`.
2. On render.com choose **New > Blueprint**, pick the repository, and approve `render.yaml`. Render builds the `Dockerfile` and gives you an `https://cambio-xxxx.onrender.com` address.
3. For your own address, open the service's **Settings > Custom Domains**, add your domain, and create the DNS record Render shows you. HTTPS is issued automatically.

Keep to one instance (rooms are stored in memory). The `starter` plan stays awake; the free plan sleeps after idle time and drops games in progress.
