# Chess Game in C

A browser-based chess game developed in **C**, using **Raylib** for graphics and **Emscripten (WebAssembly)** to run directly in a web browser.

This project demonstrates low-level programming, game development, chess logic, user interaction, and compiling native C applications for the web.

## Features

- Interactive 8×8 chessboard
- Two-player local gameplay
- Mouse-based piece selection and movement
- Chess piece movement validation
- Castling
- En passant
- Pawn promotion
- Browser-based gameplay using WebAssembly

## Technologies Used

| Technology | Purpose |
|---|---|
| C | Core game logic and chess rules |
| Raylib | Graphics rendering and mouse input |
| Emscripten | Compiling C into WebAssembly |
| WebAssembly | Running the game in a browser |
| HTML5 | Hosting the browser-based game |
| Git & GitHub | Version control |

## Project Structure

```text
circles/
├── src/
│   └── main.c          # Chess game source code
├── shell.html          # HTML template for the web build
└── out/
    ├── index.html      # Browser entry point
    ├── index.js        # Generated JavaScript
    ├── index.wasm      # Compiled WebAssembly
    └── index.data      # Packaged game assets
```

## How to Run

### Browser Version

Open the generated `out/index.html` through a local HTTP server.

For example, from the `circles` directory:

```bash
python3 -m http.server 10196 --directory out
```

Then visit:

```text
http://localhost:10196/index.html
```

### Build from Source

The project was developed using the University of the West of England's POP development environment.

From the `circles` directory, run:

```bash
/opt/pop/bin/build-wasm.sh src/main.c
```

This generates the files needed to run the game in a browser.

The build script is specific to the university environment. Building elsewhere requires a compatible Raylib and Emscripten setup.

## How to Play

1. Open the chess game in your browser.
2. Select a chess piece using your mouse.
3. Click the square where you want to move it.
4. Players alternate turns.
5. Use standard chess rules to play.

## Screenshots
![starting postion](<img width="477" height="485" alt="image" src="https://github.com/user-attachments/assets/cd4f0ca9-fbb9-4fa0-aca4-33eeeb24a0e0" />
)
![Mid Game](<img width="957" height="596" alt="image" src="https://github.com/user-attachments/assets/57fe3998-4cb6-4b39-afc6-9938e9d3947b" />)
![checked postion](<img width="957" height="599" alt="image" src="https://github.com/user-attachments/assets/6ff716ac-d649-4a80-b76b-0d6fe4dbc193" />)
![Castled Postion](<img width="959" height="599" alt="image" src="https://github.com/user-attachments/assets/df512b24-a343-4db2-a413-bab35bc45049" />)
![Checkmate](<img width="953" height="590" alt="image" src="https://github.com/user-attachments/assets/46d84ea2-6a90-47d4-a902-c76d88e81824" />)






### Gameplay
```markdown
<img width="959" height="599" alt="image" src="https://github.com/user-attachments/assets/6c62d67b-6ab9-4b29-b2dd-9bf7f3daf61f" />



```

## What I Learned

Through this project, I developed practical experience in:

- Writing and organising game logic in C.
- Representing a chessboard and managing game state.
- Implementing chess movement rules and special moves.
- Handling mouse input and graphical rendering.
- Debugging browser-based WebAssembly applications.
- Compiling native code for deployment in a web browser.
- Using Git and GitHub to manage software projects.

## Future Improvements

- Add a computer opponent.
- Implement move history and an undo feature.
- Add a game timer.
- Improve the interface for different screen sizes.
- Deploy the game publicly so it can be played without local setup.

## Author

**Ilyas Farow**

Computer Science Student — University of the West of England (UWE Bristol)

GitHub: [ilyasfarow](https://github.com/ilyasfarow)
