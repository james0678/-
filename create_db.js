// create_db.js
const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('database.db');

db.serialize(() => {
    // Create tables
    db.run(`CREATE TABLE IF NOT EXISTS tb_ph (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        value FLOAT,
        timesStamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS tb_water_level (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        value FLOAT,
        timesStamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    // Insert sample data
    const stmt1 = db.prepare("INSERT INTO tb_ph (value) VALUES (?)");
    [7.2, 7.1, 7.3, 7.0, 7.4].forEach(val => stmt1.run(val));
    stmt1.finalize();

    const stmt2 = db.prepare("INSERT INTO tb_water_level (value) VALUES (?)");
    [85.5, 84.3, 86.0, 85.2, 84.8].forEach(val => stmt2.run(val));
    stmt2.finalize();

    console.log('Database created successfully!');
});

db.close();