const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('./sensor_data.db');

// pH 테스트 데이터 삽입
function insertPhTestData() {
    const testData = {
        timestamp: new Date().toISOString(),
        sensor_id: "pH_sensor_01",
        location: "tank_1",
        pH_value: 7.1,
        voltage: 2.95
    };

    const sql = `INSERT INTO tb_ph (timestamp, sensor_id, location, pH_value, voltage) 
                 VALUES (?, ?, ?, ?, ?)`;
                 
    db.run(sql, [
        testData.timestamp,
        testData.sensor_id,
        testData.location,
        testData.pH_value,
        testData.voltage
    ], function(err) {
        if (err) {
            console.error('Error inserting pH test data:', err);
        } else {
            console.log('pH test data inserted successfully');
        }
    });
}

// Water Level 테스트 데이터 삽입
function insertWaterLevelTestData() {
    const testData = {
        timestamp: new Date().toISOString(),
        sensor_id: "water_level_01",
        location: "tank_1",
        water_level: 75.5,
        voltage: 3.3
    };

    const sql = `INSERT INTO tb_water_level (timestamp, sensor_id, location, water_level, voltage) 
                 VALUES (?, ?, ?, ?, ?)`;
                 
    db.run(sql, [
        testData.timestamp,
        testData.sensor_id,
        testData.location,
        testData.water_level,
        testData.voltage
    ], function(err) {
        if (err) {
            console.error('Error inserting water level test data:', err);
        } else {
            console.log('Water level test data inserted successfully');
        }
    });
}

// 각 테이블에 테스트 데이터 여러 개 삽입
for(let i = 0; i < 5; i++) {
    insertPhTestData();
    insertWaterLevelTestData();
}

// 연결 종료
setTimeout(() => {
    db.close();
}, 1000);