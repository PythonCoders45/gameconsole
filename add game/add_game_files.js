const express = require('express');
const bodyParser = require('body-parser');
const fs = require('fs');
const path = require('path');
const multer = require('multer');

const app = express();
const PORT = 3000;

app.use(bodyParser.json());
app.use(express.static('public'));
app.use(require('cors')());

const gamesFile = path.join(__dirname, 'games.json');

// Storage configuration for multer
const storage = multer.diskStorage({
    destination: function(req, file, cb) {
        const dir = 'uploaded_games';
        if (!fs.existsSync(dir)) fs.mkdirSync(dir);
        cb(null, dir);
    },
    filename: function(req, file, cb) {
        cb(null, Date.now() + '_' + file.originalname);
    }
});
const upload = multer({ storage: storage });

// Helper to read/write JSON
function readGames() {
    try { return JSON.parse(fs.readFileSync(gamesFile)); } 
    catch { return []; }
}
function writeGames(games) { fs.writeFileSync(gamesFile, JSON.stringify(games, null, 2)); }

// API to add game with uploaded file
app.post('/api/add_game', upload.single('gameFile'), (req, res) => {
    const { name, size, genre, description } = req.body;
    if (!name || !req.file) return res.status(400).json({ success: false, error: 'Missing name or file' });

    const games = readGames();
    const newGame = {
        id: games.length + 1,
        name,
        size: size || req.file.size + ' bytes',
        thumbnail: "", // optional
        file: req.file.path,
        genre: genre || "",
        description: description || ""
    };
    games.push(newGame);
    writeGames(games);

    res.json({ success: true, game: newGame });
});

// Middleware to protect routes
function authRequired(req, res, next) {
    const userId = req.headers['x-user-id']; // expect client to send user_id in header
    if (!userId) {
        return res.status(404).send('Not Found'); // user not signed in
    }
    next();
}

// Protect the add game page
app.use('/add_game.html', authRequired);

app.listen(PORT, () => console.log(`Game server running at http://localhost:${PORT}`));
