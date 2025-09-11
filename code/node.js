const express = require('express');
const bodyParser = require('body-parser');
const cors = require('cors');
const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors()); // Allow public access
app.use(bodyParser.json());

// In-memory store (replace with DB for real usage)
let activeCodes = {};

// Link console with user account
app.post('/api/generate_link', (req, res) => {
    const { link_code, user_id } = req.body;
    if (!link_code || !user_id) return res.status(400).json({ success: false, error: 'Missing parameters' });

    activeCodes[link_code] = { user_id, linked: true };
    console.log(`[+] Code ${link_code} linked to user ${user_id}`);
    res.json({ success: true });
});

// Console checks if code is linked
app.post('/api/link_console', (req, res) => {
    const { link_code } = req.body;
    if (!link_code) return res.status(400).json({ error: 'No code provided' });

    if (activeCodes[link_code] && activeCodes[link_code].linked) {
        return res.json({ linked: true, user_id: activeCodes[link_code].user_id });
    }

    res.json({ linked: false });
});

// Serve static files (optional for HTML site)
app.use(express.static('public'));

app.listen(PORT, () => {
    console.log(`Betnix public link server running at http://localhost:${PORT}`);
});
