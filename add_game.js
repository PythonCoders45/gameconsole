const express = require('express');
const bodyParser = require('body-parser');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = 3000;
app.use(bodyParser.json());

const gamesFile = path.join(__dirname, 'games.json');

// Helper: read current games
function readGames() {
    const data = fs.readFileSync(gamesFile, 'utf8');
    return JSON.parse(data);
}

// Helper: write games
function writeGames(games) {
    fs.writeFileSync(gamesFile, JSON.stringify(games, null, 2));
}

// API: add new game
app.post('/api/add_game', (req, res) => {
    const { name, size, thumbnail, file, genre, description } = req.body;
    if (!name || !file) return res.status(400).json({ success:false, error:'Missing required fields' });

    const games = readGames();
    const newGame = {
        id: games.length + 1,
        name, size: size || "Unknown", thumbnail: thumbnail || "", file,
        genre: genre || "", description: description || ""
    };
    games.push(newGame);
    writeGames(games);
    res.json({ success:true, game:newGame });
});

app.listen(PORT, () => console.log(`Game addition server running at http://localhost:${PORT}`));
