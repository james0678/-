require('dotenv').config();
const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const session = require('express-session');

// 환경변수 체크
if (!process.env.SESSION_SECRET) {
    console.error('❌ Error: SESSION_SECRET is not set in environment variables');
    process.exit(1);
}

if (!process.env.AUTH_TOKEN) {
    console.error('❌ Error: AUTH_TOKEN is not set in environment variables');
    process.exit(1);
}

const app = express();
const PORT = process.env.PORT || 5000;

// sensor_data.db 사용하도록 변경
const db = new sqlite3.Database('./sensor_data.db', (err) => {
    if (err) {
        console.error('❌ Database connection error:', err);
        return;
    }
    console.log('✅ Successfully connected to the database');
    initializeTables();
});

// 모든 테이블 초기화 함수
function initializeTables() {
    const tables = [
        `CREATE TABLE IF NOT EXISTS tb_ph (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            pH_value REAL,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_water_level (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            water_level REAL,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_water_temperature (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            temperature REAL,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_illuminance (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            illuminance REAL,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_conductivity (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            conductivity REAL,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_air_pump (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            status TEXT,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_water_pump (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            status TEXT,
            voltage REAL
        )`,
        `CREATE TABLE IF NOT EXISTS tb_alert (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            alert_type TEXT,
            message TEXT
        )`,
        `CREATE TABLE IF NOT EXISTS tb_feed (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            sensor_id TEXT,
            location TEXT,
            feed_amount REAL,
            status TEXT
        )`,
        `CREATE TABLE IF NOT EXISTS tb_sensor_settings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sensor_type TEXT,
            setting_name TEXT,
            setting_value REAL,
            updated_at TEXT,
            UNIQUE(sensor_type, setting_name)
        )`
    ];

    tables.forEach(sql => {
        db.run(sql, err => {
            if (err) {
                console.error('Table creation error:', err);
            }
        });
    });
}

// 세션 설정
app.use(session({
    secret: process.env.SESSION_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: { secure: false }
}));

// 인증 미들웨어
app.use((req, res, next) => {
    if (req.session.isAuthenticated) {
        return next();
    }

    const token = req.headers['authorization'];
    if (!token || token !== `Bearer ${process.env.AUTH_TOKEN}`) {
        return res.status(401).json({ error: 'Unauthorized access' });
    }
    
    req.session.isAuthenticated = true;
    next();
});

// 데이터 조회 함수
const fetchData = (tableName, params, callback) => {
    const tables = [
        'tb_ph', 'tb_water_level', 'tb_water_temperature', 
        'tb_illuminance', 'tb_conductivity', 'tb_air_pump',
        'tb_water_pump', 'tb_alert', 'tb_feed'
    ];

    if (!tables.includes(tableName)) {
        return callback(new Error('Invalid table name'), null);
    }

    let query = `SELECT *, '${tableName}' as table_name FROM ${tableName}`;
    const queryParams = [];

    if (params.start && params.end) {
        query += ` WHERE timestamp BETWEEN ? AND ?`;
        queryParams.push(params.start, params.end);
    }

    query += ` ORDER BY timestamp DESC`;
    
    if (params.limit && params.offset) {
        query += ` LIMIT ? OFFSET ?`;
        queryParams.push(parseInt(params.limit), parseInt(params.offset));
    }

    db.all(query, queryParams, callback);
};

// API 엔드포인트 생성
const createEndpoint = (tableName) => {
    app.get(`/api/${tableName}`, (req, res) => {
        const { limit, offset, start, end } = req.query;

        fetchData(tableName, { limit, offset, start, end }, (err, rows) => {
            if (err) {
                res.status(500).json({ 
                    error: `Error fetching data from ${tableName}`, 
                    details: err.message 
                });
            } else if (!rows.length) {
                res.status(404).json({ 
                    table: tableName,
                    error: `No data found in ${tableName}` 
                });
            } else {
                res.json({
                    table: tableName,
                    data: rows
                });
            }
        });
    });
};

// 모든 테이블에 대한 엔드포인트 생성
[
    'tb_ph', 'tb_water_level', 'tb_water_temperature', 
    'tb_illuminance', 'tb_conductivity', 'tb_air_pump',
    'tb_water_pump', 'tb_alert', 'tb_feed'
].forEach(createEndpoint);

// POST 요청을 위한 JSON 파서 미들웨어 추가
app.use(express.json());

// 센서 설정값 조회 API
app.get('/api/sensor-settings/:sensorType', (req, res) => {
    const { sensorType } = req.params;
    
    db.all(
        'SELECT * FROM tb_sensor_settings WHERE sensor_type = ?',
        [sensorType],
        (err, rows) => {
            if (err) {
                res.status(500).json({ error: 'Database error', details: err.message });
            } else {
                res.json({ settings: rows });
            }
        }
    );
});

// 센서 설정값 업데이트 API
app.post('/api/sensor-settings/:sensorType', (req, res) => {
    const { sensorType } = req.params;
    const { settings } = req.body;
    
    if (!settings || !Array.isArray(settings)) {
        return res.status(400).json({ error: 'Invalid settings format' });
    }

    const timestamp = new Date().toISOString();
    
    // 트랜잭션 시작
    db.serialize(() => {
        db.run('BEGIN TRANSACTION');

        try {
            settings.forEach(setting => {
                db.run(
                    `INSERT INTO tb_sensor_settings (sensor_type, setting_name, setting_value, updated_at)
                     VALUES (?, ?, ?, ?)
                     ON CONFLICT(sensor_type, setting_name) 
                     DO UPDATE SET setting_value = ?, updated_at = ?`,
                    [sensorType, setting.name, setting.value, timestamp, setting.value, timestamp]
                );
            });

            db.run('COMMIT', (err) => {
                if (err) {
                    throw err;
                }
                res.json({ 
                    message: 'Settings updated successfully',
                    sensorType,
                    timestamp 
                });
            });
        } catch (err) {
            db.run('ROLLBACK');
            res.status(500).json({ 
                error: 'Failed to update settings',
                details: err.message 
            });
        }
    });
});

// 404 에러 처리
app.use((req, res) => {
    res.status(404).json({
        error: 'Invalid endpoint or table does not exist',
        message: `The requested path '${req.path}' was not found`
    });
});

// 종료 처리
process.on('SIGINT', () => {
    db.close((err) => {
        if (err) {
            console.error('Error closing database:', err);
        }
        process.exit(err ? 1 : 0);
    });
});

// 서버 시작
app.listen(PORT, () => {
    console.log(`API server is running at http://localhost:${PORT}`);
});
