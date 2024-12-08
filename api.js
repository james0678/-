// <사용 예시>
// # 수위 센서의 level_low 설정값을 5로 변경
// curl -X PUT \
//   http://localhost:5000/api/sensor-settings/water_level/level_low \
//   -H 'Authorization: Bearer your_auth_token' \
//   -H 'Content-Type: application/json' \
//   -d '{"value": 5}'

// # 응답
// {
//   "message": "Setting updated successfully",
//   "sensor_type": "water_level",
//   "setting_name": "level_low",
//   "new_value": 5,
//   "updated_at": "2024-03-20T09:30:00.000Z"
// }


require('dotenv').config();
const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const session = require('express-session');
const path = require('path');
const fs = require('fs');

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

// photos 디렉토리 설정
const PHOTOS_DIR = path.join(__dirname, 'photos');
if (!fs.existsSync(PHOTOS_DIR)) {
    fs.mkdirSync(PHOTOS_DIR);
    console.log('✅ Photos directory created');
}

// sensor_data.db 사용하도록 변경
const db = new sqlite3.Database('./sensor_data.db', (err) => {
    if (err) {
        console.error('❌ Database connection error:', err);
        process.exit(1);
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

    // 테이블 생성 후 기본설정값 초기화
    initializeDefaultSettings();
}

// 기본 센서 설정값초기화 함수 추가
function initializeDefaultSettings() {
    const defaultSettings = [
        // 카메라 설정
        { sensor_type: 'camera', setting_name: 'capture_interval', setting_value: 3600 }, // 초 단위

        // 먹이 모터 설정
        { sensor_type: 'feed', setting_name: 'feed_interval', setting_value: 43200 }, // 12시간(초)

        // 기포 발생기 설정
        { sensor_type: 'air_pump', setting_name: 'ph_threshold_min', setting_value: 6.5 },
        { sensor_type: 'air_pump', setting_name: 'ph_threshold_max', setting_value: 7.5 },
        { sensor_type: 'air_pump', setting_name: 'conductivity_threshold_min', setting_value: 100 },
        { sensor_type: 'air_pump', setting_name: 'conductivity_threshold_max', setting_value: 500 },

        // 수위 센서 설정
        { sensor_type: 'water_level', setting_name: 'level_low', setting_value: 10 },
        { sensor_type: 'water_level', setting_name: 'level_normal', setting_value: 20 },
        { sensor_type: 'water_level', setting_name: 'level_high', setting_value: 30 },

        // pH 센서 설정
        { sensor_type: 'ph', setting_name: 'sensing_interval', setting_value: 300 }, // 5분(초)
        { sensor_type: 'ph', setting_name: 'alert_min', setting_value: 6.0 },
        { sensor_type: 'ph', setting_name: 'alert_max', setting_value: 8.0 },

        // 전도도 센서 설정
        { sensor_type: 'conductivity', setting_name: 'sensing_interval', setting_value: 300 },

        // 조도 센서 설정
        { sensor_type: 'illuminance', setting_name: 'sensing_interval', setting_value: 300 },
        { sensor_type: 'illuminance', setting_name: 'alert_max', setting_value: 1000 },

        // 수온 센서 설정
        { sensor_type: 'water_temperature', setting_name: 'sensing_interval', setting_value: 300 },
        { sensor_type: 'water_temperature', setting_name: 'alert_min', setting_value: 20 },
        { sensor_type: 'water_temperature', setting_name: 'alert_max', setting_value: 30 }
    ];

    const stmt = db.prepare(`
        INSERT OR REPLACE INTO tb_sensor_settings 
        (sensor_type, setting_name, setting_value, updated_at)
        VALUES (?, ?, ?, datetime('now'))
    `);

    db.serialize(() => {
        db.run('BEGIN TRANSACTION');
        defaultSettings.forEach(setting => {
            stmt.run(setting.sensor_type, setting.setting_name, setting.setting_value, (err) => {
                if (err) {
                    db.run('ROLLBACK');
                    console.error('Error inserting default settings:', err);
                    return;
                }
            });
        });
        db.run('COMMIT');
    });

    stmt.finalize();
}

// 세션 설정
const isProduction = process.env.NODE_ENV === 'production';
app.use(session({
    secret: process.env.SESSION_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: { 
        secure: isProduction,
        httpOnly: true,
        sameSite: 'strict'
    }
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

// POST 요청을 위한 JSON 파서 미들웨어 추가
app.use(express.json());

// 센서 데이터 저장 API 엔드포인트
app.post('/api/sensor-data/:sensorType', (req, res) => {
    const { sensorType } = req.params;
    const sensorData = req.body;
    
    // 센서 타입별 테이블 매핑
    const tableMap = {
        'ph': 'tb_ph',
        'water_level': 'tb_water_level',
        'water_temperature': 'tb_water_temperature',
        'illuminance': 'tb_illuminance',
        'conductivity': 'tb_conductivity'
    };

    const tableName = tableMap[sensorType];
    if (!tableName) {
        return res.status(400).json({ error: 'Invalid sensor type' });
    }

    // 데이터 유효성 검사 및 변환 함수
    const validateAndTransform = (data) => {
        switch(sensorType) {
            case 'ph':
                return {
                    timestamp: data.timestamp,
                    sensor_id: data.sensor_id,
                    location: data.location,
                    pH_value: parseFloat(data.pH_value),
                    voltage: parseFloat(data.voltage)
                };
            case 'water_level':
                return {
                    timestamp: data.timestamp,
                    sensor_id: data.sensor_id,
                    location: data.location,
                    water_level: parseFloat(data.water_level),
                    voltage: parseFloat(data.voltage)
                };
            case 'water_temperature':
                return {
                    timestamp: data.timestamp,
                    sensor_id: data.sensor_id,
                    location: data.location,
                    temperature: parseFloat(data.temp_value),
                    voltage: parseFloat(data.voltage || 0) // 옵셔널
                };
            case 'illuminance':
                return {
                    timestamp: data.timestamp,
                    sensor_id: data.sensor_id,
                    location: data.location,
                    illuminance: parseFloat(data.light_value),
                    voltage: parseFloat(data.voltage || 0)
                };
            case 'conductivity':
                return {
                    timestamp: data.timestamp,
                    sensor_id: data.sensor_id,
                    location: data.location,
                    conductivity: parseFloat(data.tds_value),
                    voltage: parseFloat(data.voltage || 0)
                };
            default:
                throw new Error('Invalid sensor type');
        }
    };

    try {
        const transformedData = validateAndTransform(sensorData);
        
        // 데이터베이스에 저장
        const columns = Object.keys(transformedData).join(', ');
        const placeholders = Object.keys(transformedData).map(() => '?').join(', ');
        const values = Object.values(transformedData);

        db.run(
            `INSERT INTO ${tableName} (${columns}) VALUES (${placeholders})`,
            values,
            function(err) {
                if (err) {
                    res.status(500).json({ error: 'Failed to save sensor data', details: err.message });
                } else {
                    res.json({ 
                        message: 'Sensor data saved successfully',
                        id: this.lastID
                    });
                }
            }
        );
    } catch (err) {
        res.status(400).json({ error: 'Invalid sensor data format', details: err.message });
    }
});

// 센서 설정값 조회 API
app.get('/api/sensor-settings/:sensorType', (req, res) => {
    const { sensorType } = req.params;
    
    db.all(
        'SELECT * FROM tb_sensor_settings WHERE sensor_type = ? ORDER BY setting_name',
        [sensorType],
        (err, rows) => {
            if (err) {
                res.status(500).json({ error: 'Database error', details: err.message });
            } else if (rows.length === 0) {
                res.status(404).json({ error: 'No settings found for this sensor type' });
            } else {
                res.json({ 
                    sensor_type: sensorType,
                    settings: rows 
                });
            }
        }
    );
});

// 특정 센서의 특정 설정값 조회 API
app.get('/api/sensor-settings/:sensorType/:settingName', (req, res) => {
    const { sensorType, settingName } = req.params;
    
    db.get(
        'SELECT * FROM tb_sensor_settings WHERE sensor_type = ? AND setting_name = ?',
        [sensorType, settingName],
        (err, row) => {
            if (err) {
                res.status(500).json({ error: 'Database error', details: err.message });
            } else if (!row) {
                res.status(404).json({ error: 'Setting not found' });
            } else {
                res.json(row);
            }
        }
    );
});

// 센서 설정값 업데이트 API
app.put('/api/sensor-settings/:sensorType/:settingName', (req, res) => {
    const { sensorType, settingName } = req.params;
    const { value } = req.body;
    
    // 값 유효성 검사
    if (value === undefined || value === null) {
        return res.status(400).json({ error: 'Setting value is required' });
    }

    // 숫자 값으로 변환 시도
    const numericValue = Number(value);
    if (isNaN(numericValue)) {
        return res.status(400).json({ error: 'Setting value must be a number' });
    }

    const timestamp = new Date().toISOString();

    db.run(
        `UPDATE tb_sensor_settings 
         SET setting_value = ?, updated_at = ? 
         WHERE sensor_type = ? AND setting_name = ?`,
        [numericValue, timestamp, sensorType, settingName],
        function(err) {
            if (err) {
                res.status(500).json({ 
                    error: 'Database error', 
                    details: err.message 
                });
            } else if (this.changes === 0) {
                res.status(404).json({ 
                    error: 'Setting not found',
                    message: `No setting found for sensor type '${sensorType}' with name '${settingName}'`
                });
            } else {
                res.json({ 
                    message: 'Setting updated successfully',
                    sensor_type: sensorType,
                    setting_name: settingName,
                    new_value: numericValue,
                    updated_at: timestamp
                });
            }
        }
    );
});

// 최근 사진 조회 API
app.get('/api/latest-photo', (req, res) => {
    fs.readdir(PHOTOS_DIR, (err, files) => {
        if (err) {
            return res.status(500).json({ error: 'Failed to read directory' });
        }

        // 이미지 파일만 필터링
        const imageFiles = files.filter(file => {
            const ext = path.extname(file).toLowerCase();
            return ['.jpg', '.jpeg', '.png', '.gif'].includes(ext);
        });

        if (imageFiles.length === 0) {
            return res.status(404).json({ error: 'No photos found' });
        }

        // 각 파일의 수정 시간 확인
        const fileStats = imageFiles.map(file => ({
            name: file,
            time: fs.statSync(path.join(PHOTOS_DIR, file)).mtime.getTime()
        }));

        // 수정 시간 기준 정렬
        fileStats.sort((a, b) => b.time - a.time);

        // 가장 최근 파일 전송
        const latestFile = path.join(PHOTOS_DIR, fileStats[0].name);
        res.sendFile(latestFile);
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
